#ifndef TMK_MIGRATE_PLANNER_H
#define TMK_MIGRATE_PLANNER_H

#include "../../include/tidemark/types.h"

/*
 * Migration planner — evaluates partition balance and generates
 * migration plans to rebalance data distribution.
 */

typedef struct {
    uint16_t source_partition_id;
    uint16_t dest_partition_id;
    uint16_t segments_to_move;
    float    imbalance_score;
    uint32_t estimated_cost;
} tmk_migration_plan_t;

int tmk_migrate_evaluate_balance(tmk_context_t *ctx, float *out_imbalance);
int tmk_migrate_generate_plan(tmk_context_t *ctx, tmk_migration_plan_t *plan);
int tmk_migrate_estimate_cost(tmk_context_t *ctx, uint16_t src_part,
                              uint16_t dst_part, uint32_t *out_cost);
bool tmk_migrate_should_rebalance(tmk_context_t *ctx);

#endif
