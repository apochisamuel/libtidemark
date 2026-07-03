#ifndef TMK_MATERIALIZE_FILTER_H
#define TMK_MATERIALIZE_FILTER_H

#include "../../include/tidemark/types.h"

/*
 * Materialization filter predicates for time-series queries.
 *
 * Filters are composable predicates evaluated against 8-byte
 * records [key:4][value:4].  Supports key range, value range,
 * modulo, and bitwise mask predicates.
 */

typedef enum {
    TMK_FILTER_KEY_RANGE   = 0,
    TMK_FILTER_VALUE_RANGE = 1,
    TMK_FILTER_KEY_MODULO  = 2,
    TMK_FILTER_VALUE_MASK  = 3,
    TMK_FILTER_EPOCH_RANGE = 4
} tmk_filter_type_t;

typedef struct {
    tmk_filter_type_t type;
    uint32_t          param_a;   /* low bound / modulus / mask */
    uint32_t          param_b;   /* high bound / remainder / expected */
} tmk_filter_predicate_t;

#define TMK_MAX_FILTERS 8

typedef struct {
    tmk_filter_predicate_t predicates[TMK_MAX_FILTERS];
    int                    count;
    bool                   conjunction; /* true=AND, false=OR */
} tmk_filter_set_t;

int  tmk_filter_init(tmk_filter_set_t *fs, bool conjunction);
int  tmk_filter_add(tmk_filter_set_t *fs, tmk_filter_type_t type,
                    uint32_t param_a, uint32_t param_b);
bool tmk_filter_evaluate_record(const tmk_filter_set_t *fs,
                                uint32_t key, uint32_t value);
int  tmk_filter_scan_segment(tmk_context_t *ctx, uint16_t segment_id,
                             const tmk_filter_set_t *fs,
                             uint8_t *out, size_t out_cap,
                             size_t *out_len);
int  tmk_filter_count_matching(tmk_context_t *ctx, uint16_t segment_id,
                               const tmk_filter_set_t *fs);

#endif
