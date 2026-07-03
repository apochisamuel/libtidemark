/*
 * libtidemark — aggregation operations
 *
 * Computes streaming aggregates over segment data using Welford's
 * online algorithm for numerically stable variance computation.
 *
 * For partition-level aggregates, results are computed per-segment
 * and then merged using the parallel variance merge algorithm:
 *   combined_mean = (n_a * mean_a + n_b * mean_b) / (n_a + n_b)
 *   combined_var  = (n_a * (var_a + d_a^2) + n_b * (var_b + d_b^2)) / (n_a + n_b)
 *   where d_a = mean_a - combined_mean, d_b = mean_b - combined_mean
 */
#include "aggregate.h"
#include "../session/manager.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>
#include <math.h>

static inline uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tmk_materialize_aggregate_segment(tmk_context_t *ctx, uint16_t segment_id,
                                      tmk_aggregate_result_t *result) {
    if (!result) return TMK_ERR_SHORT_BUF;
    memset(result, 0, sizeof(*result));
    result->min_val = UINT32_MAX;
    result->min_key = UINT32_MAX;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    /* Welford's online algorithm */
    double mean = 0.0;
    double m2 = 0.0;
    uint64_t n = 0;

    for (size_t offset = 0; offset + 8 <= seg->used; offset += 8) {
        uint32_t key = read_u32_le(seg->data + offset);
        uint32_t val = read_u32_le(seg->data + offset + 4);

        n++;
        result->sum += val;

        if (val < result->min_val) result->min_val = val;
        if (val > result->max_val) result->max_val = val;
        if (key < result->min_key) result->min_key = key;
        if (key > result->max_key) result->max_key = key;

        /* Welford update */
        double delta = (double)val - mean;
        mean += delta / (double)n;
        double delta2 = (double)val - mean;
        m2 += delta * delta2;
    }

    result->count = n;
    result->mean = mean;
    if (n > 1) {
        result->variance = m2 / (double)(n - 1);
        result->stddev = sqrt(result->variance);
    }

    return TMK_OK;
}

int tmk_materialize_aggregate_partition(tmk_context_t *ctx, uint16_t partition_id,
                                        tmk_aggregate_result_t *result) {
    if (!result) return TMK_ERR_SHORT_BUF;
    memset(result, 0, sizeof(*result));
    result->min_val = UINT32_MAX;
    result->min_key = UINT32_MAX;

    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    for (uint16_t i = 0; i < part->segment_count; i++) {
        tmk_aggregate_result_t seg_result;
        int rc = tmk_materialize_aggregate_segment(ctx,
                    part->segment_ids[i], &seg_result);
        if (rc < 0) continue;

        tmk_materialize_aggregate_merge(result, &seg_result);
    }

    return TMK_OK;
}

int tmk_materialize_aggregate_range(tmk_context_t *ctx, uint16_t segment_id,
                                    uint32_t key_lo, uint32_t key_hi,
                                    tmk_aggregate_result_t *result) {
    if (!result) return TMK_ERR_SHORT_BUF;
    memset(result, 0, sizeof(*result));
    result->min_val = UINT32_MAX;
    result->min_key = UINT32_MAX;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    double mean = 0.0;
    double m2 = 0.0;
    uint64_t n = 0;

    for (size_t offset = 0; offset + 8 <= seg->used; offset += 8) {
        uint32_t key = read_u32_le(seg->data + offset);
        uint32_t val = read_u32_le(seg->data + offset + 4);

        if (key < key_lo || key > key_hi) continue;

        n++;
        result->sum += val;

        if (val < result->min_val) result->min_val = val;
        if (val > result->max_val) result->max_val = val;
        if (key < result->min_key) result->min_key = key;
        if (key > result->max_key) result->max_key = key;

        double delta = (double)val - mean;
        mean += delta / (double)n;
        double delta2 = (double)val - mean;
        m2 += delta * delta2;
    }

    result->count = n;
    result->mean = mean;
    if (n > 1) {
        result->variance = m2 / (double)(n - 1);
        result->stddev = sqrt(result->variance);
    }

    return TMK_OK;
}

int tmk_materialize_aggregate_merge(tmk_aggregate_result_t *dst,
                                    const tmk_aggregate_result_t *src) {
    if (!dst || !src) return TMK_ERR_SHORT_BUF;
    if (src->count == 0) return TMK_OK;

    if (dst->count == 0) {
        *dst = *src;
        return TMK_OK;
    }

    /* Parallel merge of means and variances */
    uint64_t n_total = dst->count + src->count;
    double combined_mean = ((double)dst->count * dst->mean +
                            (double)src->count * src->mean) / (double)n_total;

    double d_dst = dst->mean - combined_mean;
    double d_src = src->mean - combined_mean;
    double combined_var = ((double)dst->count * (dst->variance + d_dst * d_dst) +
                           (double)src->count * (src->variance + d_src * d_src)) /
                          (double)n_total;

    dst->count = n_total;
    dst->sum += src->sum;
    dst->mean = combined_mean;
    dst->variance = combined_var;
    dst->stddev = sqrt(combined_var);

    if (src->min_val < dst->min_val) dst->min_val = src->min_val;
    if (src->max_val > dst->max_val) dst->max_val = src->max_val;
    if (src->min_key < dst->min_key) dst->min_key = src->min_key;
    if (src->max_key > dst->max_key) dst->max_key = src->max_key;

    return TMK_OK;
}

/*
 * Index-accelerated aggregate scan.
 * Uses the index node's stored_capacity for scan bounds.
 *
 * BUG: After compact_merge shrinks a segment, index_node.stored_capacity
 * still reflects the OLD (larger) capacity.  The scan reads past the
 * new buffer's actual allocation → heap-buffer-overflow.
 */
int tmk_materialize_aggregate_indexed(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t idx_id = frame->header.route_id;
    tmk_index_node_t *idx = tmk_find_index_node(ctx, idx_id);
    if (!idx || !idx->active || !idx->segment_ref) return TMK_ERR_NOT_FOUND;

    tmk_segment_t *seg = idx->segment_ref;
    if (!seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    /* BUG: use stored_capacity (stale after merge) instead of seg->capacity */
    size_t scan_limit = idx->stored_capacity;

    /* Scan records using the (potentially stale) capacity bound */
    uint64_t sum = 0;
    for (size_t offset = 0; offset + 8 <= scan_limit; offset += 8) {
        /* READ PAST BUFFER if scan_limit > seg->capacity after merge */
        volatile uint8_t k0 = seg->data[offset];
        volatile uint8_t k1 = seg->data[offset + 4];
        sum += k0 + k1;
        (void)sum;
    }

    return TMK_OK;
}
