/*
 * libtidemark — query filter evaluation
 *
 * Composable predicate evaluation over segment data.  Predicates
 * are evaluated in the order added; short-circuit logic applies
 * (AND short-circuits on first false, OR on first true).
 *
 * Key range: key_min <= key <= key_max
 * Value range: val_min <= value <= val_max
 * Key modulo: key % modulus == remainder
 * Value mask: (value & mask) == expected
 * Epoch range: create_epoch in [lo, hi] (segment-level, not per-record)
 */
#include "filter.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>

static inline uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tmk_filter_init(tmk_filter_set_t *fs, bool conjunction) {
    if (!fs) return TMK_ERR_SHORT_BUF;
    memset(fs, 0, sizeof(*fs));
    fs->conjunction = conjunction;
    return TMK_OK;
}

int tmk_filter_add(tmk_filter_set_t *fs, tmk_filter_type_t type,
                   uint32_t param_a, uint32_t param_b) {
    if (!fs || fs->count >= TMK_MAX_FILTERS) return TMK_ERR_OVERFLOW;
    fs->predicates[fs->count].type = type;
    fs->predicates[fs->count].param_a = param_a;
    fs->predicates[fs->count].param_b = param_b;
    fs->count++;
    return TMK_OK;
}

static bool eval_predicate(const tmk_filter_predicate_t *pred,
                           uint32_t key, uint32_t value) {
    switch (pred->type) {
    case TMK_FILTER_KEY_RANGE:
        return key >= pred->param_a && key <= pred->param_b;
    case TMK_FILTER_VALUE_RANGE:
        return value >= pred->param_a && value <= pred->param_b;
    case TMK_FILTER_KEY_MODULO:
        if (pred->param_a == 0) return false;
        return (key % pred->param_a) == pred->param_b;
    case TMK_FILTER_VALUE_MASK:
        return (value & pred->param_a) == pred->param_b;
    case TMK_FILTER_EPOCH_RANGE:
        /* Epoch filtering is segment-level; always true here */
        return true;
    default:
        return true;
    }
}

bool tmk_filter_evaluate_record(const tmk_filter_set_t *fs,
                                uint32_t key, uint32_t value) {
    if (!fs || fs->count == 0) return true;

    if (fs->conjunction) {
        /* AND: all must match */
        for (int i = 0; i < fs->count; i++) {
            if (!eval_predicate(&fs->predicates[i], key, value))
                return false;
        }
        return true;
    } else {
        /* OR: any must match */
        for (int i = 0; i < fs->count; i++) {
            if (eval_predicate(&fs->predicates[i], key, value))
                return true;
        }
        return false;
    }
}

int tmk_filter_scan_segment(tmk_context_t *ctx, uint16_t segment_id,
                            const tmk_filter_set_t *fs,
                            uint8_t *out, size_t out_cap,
                            size_t *out_len) {
    if (!fs || !out || !out_len) return TMK_ERR_SHORT_BUF;
    *out_len = 0;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    /* Epoch-level filter check */
    for (int i = 0; i < fs->count; i++) {
        if (fs->predicates[i].type == TMK_FILTER_EPOCH_RANGE) {
            if (seg->create_epoch < fs->predicates[i].param_a ||
                seg->create_epoch > fs->predicates[i].param_b) {
                return TMK_OK; /* entire segment filtered out */
            }
        }
    }

    /* Scan records */
    for (size_t offset = 0; offset + 8 <= seg->used &&
         *out_len + 8 <= out_cap; offset += 8) {
        uint32_t key = rd32(seg->data + offset);
        uint32_t val = rd32(seg->data + offset + 4);

        if (tmk_filter_evaluate_record(fs, key, val)) {
            memcpy(out + *out_len, seg->data + offset, 8);
            *out_len += 8;
        }
    }

    return TMK_OK;
}

int tmk_filter_count_matching(tmk_context_t *ctx, uint16_t segment_id,
                              const tmk_filter_set_t *fs) {
    if (!fs) return 0;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return 0;

    int count = 0;
    for (size_t offset = 0; offset + 8 <= seg->used; offset += 8) {
        uint32_t key = rd32(seg->data + offset);
        uint32_t val = rd32(seg->data + offset + 4);

        if (tmk_filter_evaluate_record(fs, key, val)) {
            count++;
        }
    }

    return count;
}
