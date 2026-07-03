/*
 * libtidemark — sorted merge and record operations
 *
 * Merge-sort of two sorted segment data regions.  Each "record"
 * is 8 bytes: [key:4 LE][timestamp:4 LE].
 *
 * The merge is a standard two-pointer sweep with deduplication:
 * when both inputs have the same key, the record with the higher
 * timestamp wins (last-write-wins conflict resolution).
 *
 * The sort uses insertion sort (appropriate for the small record
 * counts typical of embedded segment sizes).  For segments with
 * many records, a merge-sort pass over 8-byte chunks would be
 * more efficient, but the typical segment has <8192 records.
 */
#include "sorted_merge.h"
#include "../../include/tidemark/error.h"
#include <string.h>

static inline uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int tmk_segment_sorted_merge(const uint8_t *data_a, size_t len_a,
                             const uint8_t *data_b, size_t len_b,
                             uint8_t *out, size_t out_cap,
                             size_t *out_len) {
    if (!data_a || !data_b || !out || !out_len)
        return TMK_ERR_SHORT_BUF;

    size_t ia = 0, ib = 0, wo = 0;

    while (ia < len_a && ib < len_b && wo + 8 <= out_cap) {
        if (ia + 8 > len_a) break;
        if (ib + 8 > len_b) break;

        uint32_t ka = rd32(data_a + ia);
        uint32_t kb = rd32(data_b + ib);

        if (ka < kb) {
            memcpy(out + wo, data_a + ia, 8);
            ia += 8;
        } else if (ka > kb) {
            memcpy(out + wo, data_b + ib, 8);
            ib += 8;
        } else {
            /* Same key: keep higher timestamp */
            uint32_t ta = rd32(data_a + ia + 4);
            uint32_t tb = rd32(data_b + ib + 4);
            if (ta >= tb) {
                memcpy(out + wo, data_a + ia, 8);
            } else {
                memcpy(out + wo, data_b + ib, 8);
            }
            ia += 8;
            ib += 8;
        }
        wo += 8;
    }

    /* Drain remaining */
    while (ia + 8 <= len_a && wo + 8 <= out_cap) {
        memcpy(out + wo, data_a + ia, 8);
        ia += 8;
        wo += 8;
    }
    while (ib + 8 <= len_b && wo + 8 <= out_cap) {
        memcpy(out + wo, data_b + ib, 8);
        ib += 8;
        wo += 8;
    }

    *out_len = wo;
    return TMK_OK;
}

int tmk_segment_sort_records(uint8_t *data, size_t len) {
    if (!data || len == 0) return TMK_OK;

    size_t nrec = len / 8;
    if (nrec <= 1) return TMK_OK;

    /* Insertion sort on 8-byte records by key */
    uint8_t tmp[8];
    for (size_t i = 1; i < nrec; i++) {
        memcpy(tmp, data + i * 8, 8);
        uint32_t key = rd32(tmp);

        size_t j = i;
        while (j > 0 && rd32(data + (j - 1) * 8) > key) {
            memcpy(data + j * 8, data + (j - 1) * 8, 8);
            j--;
        }
        memcpy(data + j * 8, tmp, 8);
    }

    return TMK_OK;
}

int tmk_segment_dedup_records(uint8_t *data, size_t len, size_t *new_len) {
    if (!data || !new_len) return TMK_ERR_SHORT_BUF;
    if (len < 16) { *new_len = len; return TMK_OK; }

    size_t nrec = len / 8;
    size_t write_pos = 0;

    for (size_t i = 0; i < nrec; i++) {
        uint32_t key = rd32(data + i * 8);
        uint32_t ts = rd32(data + i * 8 + 4);

        /* Look ahead for duplicates */
        uint32_t best_ts = ts;
        size_t best_idx = i;
        while (i + 1 < nrec && rd32(data + (i + 1) * 8) == key) {
            i++;
            uint32_t next_ts = rd32(data + i * 8 + 4);
            if (next_ts > best_ts) {
                best_ts = next_ts;
                best_idx = i;
            }
        }

        /* Write the winner */
        if (write_pos != best_idx * 8) {
            memcpy(data + write_pos, data + best_idx * 8, 8);
        }
        write_pos += 8;
    }

    *new_len = write_pos;
    return TMK_OK;
}

int tmk_segment_bsearch_key(const uint8_t *data, size_t len,
                            uint32_t key, size_t *out_offset) {
    if (!data || !out_offset || len < 8) return TMK_ERR_NOT_FOUND;

    size_t nrec = len / 8;
    size_t lo = 0, hi = nrec;

    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        uint32_t mid_key = rd32(data + mid * 8);

        if (mid_key < key) {
            lo = mid + 1;
        } else if (mid_key > key) {
            hi = mid;
        } else {
            *out_offset = mid * 8;
            return TMK_OK;
        }
    }

    return TMK_ERR_NOT_FOUND;
}

int tmk_segment_distinct_keys(const uint8_t *data, size_t len) {
    if (!data || len < 8) return 0;

    size_t nrec = len / 8;
    int distinct = 1;
    uint32_t prev = rd32(data);

    for (size_t i = 1; i < nrec; i++) {
        uint32_t cur = rd32(data + i * 8);
        if (cur != prev) {
            distinct++;
            prev = cur;
        }
    }

    return distinct;
}
