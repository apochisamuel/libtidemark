#ifndef TMK_MATERIALIZE_AGGREGATE_H
#define TMK_MATERIALIZE_AGGREGATE_H

#include "../../include/tidemark/types.h"

/*
 * Materialization aggregate operations.
 * Computes summary statistics over segment data for time-series
 * query results: count, sum, min, max, mean, variance.
 *
 * Operates on 8-byte records: [key:4][value:4]
 * where value is interpreted as a uint32 for aggregation.
 */

typedef struct {
    uint64_t count;
    uint64_t sum;
    uint32_t min_val;
    uint32_t max_val;
    uint32_t min_key;
    uint32_t max_key;
    double   mean;
    double   variance;
    double   stddev;
} tmk_aggregate_result_t;

int tmk_materialize_aggregate_segment(tmk_context_t *ctx, uint16_t segment_id,
                                      tmk_aggregate_result_t *result);
int tmk_materialize_aggregate_partition(tmk_context_t *ctx, uint16_t partition_id,
                                        tmk_aggregate_result_t *result);
int tmk_materialize_aggregate_range(tmk_context_t *ctx, uint16_t segment_id,
                                    uint32_t key_lo, uint32_t key_hi,
                                    tmk_aggregate_result_t *result);
int tmk_materialize_aggregate_merge(tmk_aggregate_result_t *dst,
                                    const tmk_aggregate_result_t *src);

int tmk_materialize_aggregate_indexed(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
#endif
