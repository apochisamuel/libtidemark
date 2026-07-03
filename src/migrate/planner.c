/*
 * libtidemark — migration planning
 *
 * Evaluates partition balance using coefficient of variation (CV)
 * of segment counts across active partitions.  If CV exceeds a
 * threshold, generates a migration plan to move segments from the
 * most-loaded to the least-loaded partition.
 *
 * Cost estimation considers:
 * - Number of segments to transfer
 * - Total data bytes involved
 * - Number of active leases on source segments
 * - Cross-partition index rebuilding cost
 */
#include "planner.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <math.h>

int tmk_migrate_evaluate_balance(tmk_context_t *ctx, float *out_imbalance) {
    if (!out_imbalance) return TMK_ERR_SHORT_BUF;

    int active = 0;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (!ctx->partitions[i].active) continue;
        active++;
        double count = (double)ctx->partitions[i].segment_count;
        sum += count;
        sum_sq += count * count;
    }

    if (active < 2) {
        *out_imbalance = 0.0f;
        return TMK_OK;
    }

    double mean = sum / active;
    double variance = (sum_sq / active) - (mean * mean);
    double stddev = sqrt(variance > 0 ? variance : 0);

    /* Coefficient of variation */
    *out_imbalance = (mean > 0) ? (float)(stddev / mean) : 0.0f;
    return TMK_OK;
}

int tmk_migrate_generate_plan(tmk_context_t *ctx, tmk_migration_plan_t *plan) {
    if (!plan) return TMK_ERR_SHORT_BUF;

    /* Find most-loaded and least-loaded partitions */
    int max_idx = -1, min_idx = -1;
    uint16_t max_count = 0, min_count = UINT16_MAX;

    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (!ctx->partitions[i].active) continue;

        if (ctx->partitions[i].segment_count > max_count) {
            max_count = ctx->partitions[i].segment_count;
            max_idx = i;
        }
        if (ctx->partitions[i].segment_count < min_count) {
            min_count = ctx->partitions[i].segment_count;
            min_idx = i;
        }
    }

    if (max_idx < 0 || min_idx < 0 || max_idx == min_idx)
        return TMK_ERR_NOT_FOUND;

    plan->source_partition_id = ctx->partitions[max_idx].id;
    plan->dest_partition_id = ctx->partitions[min_idx].id;

    /* Move enough segments to balance */
    uint16_t diff = max_count - min_count;
    plan->segments_to_move = diff / 2;
    if (plan->segments_to_move == 0) plan->segments_to_move = 1;

    tmk_migrate_evaluate_balance(ctx, &plan->imbalance_score);
    tmk_migrate_estimate_cost(ctx, plan->source_partition_id,
                              plan->dest_partition_id, &plan->estimated_cost);

    return TMK_OK;
}

int tmk_migrate_estimate_cost(tmk_context_t *ctx, uint16_t src_part,
                              uint16_t dst_part, uint32_t *out_cost) {
    if (!out_cost) return TMK_ERR_SHORT_BUF;

    tmk_partition_t *src = tmk_find_partition(ctx, src_part);
    tmk_partition_t *dst = tmk_find_partition(ctx, dst_part);
    if (!src || !dst) { *out_cost = UINT32_MAX; return TMK_ERR_NOT_FOUND; }

    uint32_t cost = 0;

    /* Base cost: 100 per segment */
    cost += (uint32_t)src->segment_count * 100;

    /* Data cost: 1 per 1KB of data */
    for (uint16_t i = 0; i < src->segment_count; i++) {
        tmk_segment_t *seg = tmk_find_segment(ctx, src->segment_ids[i]);
        if (seg && seg->occupied) {
            cost += (uint32_t)(seg->used / 1024);
        }
    }

    /* Lease cost: 500 per active lease */
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (!ctx->leases[i].active) continue;
        tmk_segment_t *seg = tmk_find_segment(ctx, ctx->leases[i].segment_id);
        if (seg && seg->partition_id == src_part) {
            cost += 500;
        }
    }

    /* Channel rebinding cost: 200 per channel */
    cost += (uint32_t)src->channel_count * 200;

    *out_cost = cost;
    return TMK_OK;
}

bool tmk_migrate_should_rebalance(tmk_context_t *ctx) {
    float imbalance;
    tmk_migrate_evaluate_balance(ctx, &imbalance);
    return imbalance > 0.5f; /* CV > 50% triggers rebalance */
}
