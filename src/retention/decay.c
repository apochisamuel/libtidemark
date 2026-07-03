/*
 * libtidemark — retention decay curve evaluation
 *
 * Implements four decay models for time-based retention priority:
 *
 * Linear: simple proportional decay over the TTL window.
 *   Good for uniform workloads where all data ages at the same rate.
 *
 * Exponential: rapid initial decay with long tail.
 *   Lambda = ln(2) / half_life, where half_life = ttl / 2.
 *   Good for hot/cold tiering where recent data is much more valuable.
 *
 * Step: binary alive/dead at TTL boundary.
 *   Simplest model — data is fully retained until TTL, then immediately
 *   eligible for compaction.  Used by default.
 *
 * Logarithmic: slow initial decay, faster later.
 *   Good for append-heavy workloads where old data retains value
 *   longer than exponential suggests.
 */
#include "decay.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <math.h>

int tmk_decay_init(tmk_decay_config_t *cfg, tmk_decay_model_t model,
                   double ttl, double threshold) {
    if (!cfg || ttl <= 0.0) return TMK_ERR_BAD_STATE;

    cfg->model = model;
    cfg->ttl = ttl;
    cfg->threshold = threshold;

    /* Compute lambda for exponential: half-life at ttl/2 */
    cfg->lambda = log(2.0) / (ttl / 2.0);

    return TMK_OK;
}

double tmk_decay_evaluate(const tmk_decay_config_t *cfg, double age) {
    if (!cfg || age < 0.0) return 1.0;

    switch (cfg->model) {
    case TMK_DECAY_LINEAR:
        if (age >= cfg->ttl) return 0.0;
        return 1.0 - (age / cfg->ttl);

    case TMK_DECAY_EXPONENTIAL:
        return exp(-cfg->lambda * age);

    case TMK_DECAY_STEP:
        return (age < cfg->ttl) ? 1.0 : 0.0;

    case TMK_DECAY_LOGARITHMIC:
        if (age >= cfg->ttl) return 0.0;
        if (cfg->ttl <= 1.0) return 0.0;
        return 1.0 - log(1.0 + age) / log(1.0 + cfg->ttl);

    default:
        return 1.0;
    }
}

bool tmk_decay_is_expired(const tmk_decay_config_t *cfg, double age) {
    return tmk_decay_evaluate(cfg, age) < cfg->threshold;
}

int tmk_decay_evaluate_segment(tmk_context_t *ctx,
                               const tmk_decay_config_t *cfg,
                               uint16_t segment_id, double *out_priority) {
    if (!cfg || !out_priority) return TMK_ERR_SHORT_BUF;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) {
        *out_priority = 0.0;
        return TMK_ERR_NOT_FOUND;
    }

    double age = (double)(ctx->global_epoch - seg->create_epoch);
    *out_priority = tmk_decay_evaluate(cfg, age);
    return TMK_OK;
}

int tmk_decay_evaluate_partition(tmk_context_t *ctx,
                                 const tmk_decay_config_t *cfg,
                                 uint16_t partition_id,
                                 double *out_avg_priority) {
    if (!cfg || !out_avg_priority) return TMK_ERR_SHORT_BUF;

    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part || !part->active) {
        *out_avg_priority = 0.0;
        return TMK_ERR_NOT_FOUND;
    }

    double sum = 0.0;
    int count = 0;

    for (uint16_t i = 0; i < part->segment_count; i++) {
        double priority;
        int rc = tmk_decay_evaluate_segment(ctx, cfg,
                    part->segment_ids[i], &priority);
        if (rc == TMK_OK) {
            sum += priority;
            count++;
        }
    }

    *out_avg_priority = (count > 0) ? (sum / count) : 0.0;
    return TMK_OK;
}

int tmk_decay_find_expired(tmk_context_t *ctx,
                           const tmk_decay_config_t *cfg,
                           uint16_t *out_ids, uint16_t max_count,
                           uint16_t *out_count) {
    if (!cfg || !out_ids || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    for (int i = 0; i < TMK_MAX_SEGMENTS && *out_count < max_count; i++) {
        if (!ctx->segments[i].occupied) continue;
        if (ctx->segments[i].phase != TMK_PHASE_ACTIVE) continue;

        double age = (double)(ctx->global_epoch - ctx->segments[i].create_epoch);
        if (tmk_decay_is_expired(cfg, age)) {
            out_ids[*out_count] = ctx->segments[i].id;
            (*out_count)++;
        }
    }

    return TMK_OK;
}
