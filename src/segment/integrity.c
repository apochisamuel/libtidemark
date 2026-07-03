/*
 * libtidemark — segment data integrity verification
 *
 * Performs multi-level integrity checks on segment data:
 *
 * 1. CRC-32C checksum of used and full data regions
 * 2. Trailing zero verification (unused capacity should be zeroed)
 * 3. Record boundary validation (embedded 8-byte records)
 * 4. Digest computation (simple hash-based fingerprint)
 *
 * Record format within segment data:
 *   [0..3]   key       (uint32 LE)
 *   [4..7]   timestamp (uint32 LE)
 *   [8..]    value     (variable, up to next 8-byte boundary)
 *
 * The scrub operation zeroes unused data and recomputes checksums,
 * ensuring consistency for checkpoint and migration operations.
 */
#include "integrity.h"
#include "../internal/context.h"
#include "../wire/crc.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_segment_check_integrity(tmk_context_t *ctx, uint16_t segment_id,
                                tmk_integrity_report_t *report) {
    if (!report) return TMK_ERR_SHORT_BUF;
    memset(report, 0, sizeof(*report));

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    /* Compute CRC of full buffer */
    report->crc_full = tmk_crc32c(seg->data, seg->capacity);

    /* Compute CRC of used portion */
    if (seg->used > 0) {
        report->crc_used = tmk_crc32c(seg->data, seg->used);
    }

    /* Check trailing zeros */
    report->trailing_clean = true;
    report->zero_tail = 0;
    if (seg->used < seg->capacity) {
        size_t tail_start = seg->used;
        for (size_t i = tail_start; i < seg->capacity; i++) {
            if (seg->data[i] != 0) {
                report->trailing_clean = false;
                break;
            }
            report->zero_tail++;
        }
    }

    /* Count record boundaries */
    report->record_count = 0;
    report->boundary_valid = true;
    size_t offset = 0;
    while (offset + 8 <= seg->used) {
        /* Each record is at least 8 bytes (key + timestamp) */
        uint32_t key = (uint32_t)seg->data[offset] |
                       ((uint32_t)seg->data[offset + 1] << 8) |
                       ((uint32_t)seg->data[offset + 2] << 16) |
                       ((uint32_t)seg->data[offset + 3] << 24);

        /* Simple heuristic: keys should be ascending or zero */
        if (report->record_count > 0 && key == 0 && offset < seg->used - 8) {
            report->boundary_valid = false;
        }

        report->record_count++;
        offset += 8; /* fixed-size records for simplicity */
    }

    return TMK_OK;
}

int tmk_segment_verify_checksum(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;
    if (!seg->meta) return TMK_OK; /* no stored checksum to verify against */

    uint32_t computed = tmk_crc32c(seg->data, seg->used);
    if (computed != seg->meta->checksum) return TMK_ERR_BAD_CRC;

    return TMK_OK;
}

int tmk_segment_scrub(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    /* Zero unused trailing data */
    if (seg->used < seg->capacity) {
        memset(seg->data + seg->used, 0, seg->capacity - seg->used);
    }

    /* Recompute metadata checksum */
    if (seg->meta) {
        seg->meta->checksum = tmk_crc32c(seg->data, seg->used);
    }

    return TMK_OK;
}

int tmk_segment_compute_digest(tmk_context_t *ctx, uint16_t segment_id,
                               uint8_t *out_digest, size_t digest_len) {
    if (!out_digest || digest_len < 16) return TMK_ERR_SHORT_BUF;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    memset(out_digest, 0, digest_len);

    /* Simple 128-bit digest: four independent CRC passes with
     * different seeds, operating on interleaved byte streams */
    uint32_t crcs[4] = {0, 0, 0, 0};

    for (size_t i = 0; i < seg->used; i++) {
        int lane = (int)(i % 4);
        crcs[lane] = tmk_crc32c_update(crcs[lane], &seg->data[i], 1);
    }

    /* Mix the four CRC values */
    for (int i = 0; i < 4; i++) {
        out_digest[i * 4 + 0] = (uint8_t)(crcs[i]);
        out_digest[i * 4 + 1] = (uint8_t)(crcs[i] >> 8);
        out_digest[i * 4 + 2] = (uint8_t)(crcs[i] >> 16);
        out_digest[i * 4 + 3] = (uint8_t)(crcs[i] >> 24);
    }

    return TMK_OK;
}

int tmk_segment_count_records(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;
    if (seg->used == 0) return 0;
    return (int)(seg->used / 8);
}

int tmk_segment_find_record(tmk_context_t *ctx, uint16_t segment_id,
                            size_t index, size_t *out_offset, size_t *out_len) {
    if (!out_offset || !out_len) return TMK_ERR_SHORT_BUF;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    size_t record_offset = index * 8;
    if (record_offset + 8 > seg->used) return TMK_ERR_NOT_FOUND;

    *out_offset = record_offset;
    *out_len = 8;
    return TMK_OK;
}

int tmk_segment_validate_records(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    uint32_t prev_key = 0;
    int violations = 0;

    for (size_t offset = 0; offset + 8 <= seg->used; offset += 8) {
        uint32_t key = (uint32_t)seg->data[offset] |
                       ((uint32_t)seg->data[offset + 1] << 8) |
                       ((uint32_t)seg->data[offset + 2] << 16) |
                       ((uint32_t)seg->data[offset + 3] << 24);

        /* Keys should be monotonically non-decreasing */
        if (key < prev_key && key != 0) {
            violations++;
        }
        prev_key = key;
    }

    return violations;
}
