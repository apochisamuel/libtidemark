/*
 * libtidemark — compaction strategy evaluation
 *
 * Implements four compaction strategies, each optimizing for a
 * different workload characteristic.  The scoring functions
 * produce normalized [0.0, 1.0] scores; the strategy with the
 * highest aggregate score across its candidates is selected.
 *
 * Size-tiered: score = 1.0 - |size_a - size_b| / max(size_a, size_b)
 *   Favors merging segments of similar size to avoid write amplification.
 *
 * Leveled: score = generation_age / max_generations
 *   Favors promoting old segments to higher levels.
 *
 * Time-windowed: score = epoch_density (records per epoch)
 *   Favors merging segments covering similar time windows.
 *
 * Tombstone: score = tombstone_fraction
 *   Favors reclaiming space from heavily-deleted segments.
 */
#include "strategy.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"

int tmk_compact_score_size_tiered(tmk_context_t *ctx,
                                  uint16_t seg_a, uint16_t seg_b,
                                  float *out_score) {
    if (!out_score) return TMK_ERR_SHORT_BUF;

    tmk_segment_t *a = tmk_find_segment(ctx, seg_a);
    tmk_segment_t *b = tmk_find_segment(ctx, seg_b);
    if (!a || !b || !a->occupied || !b->occupied) {
        *out_score = 0.0f;
        return TMK_ERR_NOT_FOUND;
    }

    size_t max_size = (a->used > b->used) ? a->used : b->used;
    if (max_size == 0) {
        *out_score = 1.0f;
        return TMK_OK;
    }

    size_t diff = (a->used > b->used) ? (a->used - b->used) : (b->used - a->used);
    *out_score = 1.0f - (float)diff / (float)max_size;

    return TMK_OK;
}

int tmk_compact_score_leveled(tmk_context_t *ctx,
                              uint16_t segment_id, float *out_score) {
    if (!out_score) return TMK_ERR_SHORT_BUF;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) {
        *out_score = 0.0f;
        return TMK_ERR_NOT_FOUND;
    }

    uint32_t gen_age = ctx->global_generation - seg->generation;
    if (ctx->global_generation == 0) {
        *out_score = 0.0f;
    } else {
        *out_score = (float)gen_age / (float)ctx->global_generation;
        if (*out_score > 1.0f) *out_score = 1.0f;
    }

    return TMK_OK;
}

int tmk_compact_score_time_window(tmk_context_t *ctx,
                                  uint16_t segment_id, float *out_score) {
    if (!out_score) return TMK_ERR_SHORT_BUF;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) {
        *out_score = 0.0f;
        return TMK_ERR_NOT_FOUND;
    }

    uint32_t epoch_age = ctx->global_epoch - seg->create_epoch;
    if (ctx->global_epoch == 0) {
        *out_score = 0.0f;
    } else {
        /* Favor segments created at similar epochs */
        float normalized_age = (float)epoch_age / (float)ctx->global_epoch;
        *out_score = normalized_age;
        if (*out_score > 1.0f) *out_score = 1.0f;
    }

    return TMK_OK;
}

int tmk_compact_score_tombstone(tmk_context_t *ctx,
                                uint16_t segment_id, float *out_score) {
    if (!out_score) return TMK_ERR_SHORT_BUF;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) {
        *out_score = 0.0f;
        return TMK_ERR_NOT_FOUND;
    }

    *out_score = seg->tombstone ? 1.0f : 0.0f;
    return TMK_OK;
}

tmk_compact_strategy_t tmk_compact_select_strategy(tmk_context_t *ctx) {
    /* Count tombstones; if many, prioritize tombstone compaction */
    if (ctx->tombstone_count >= TMK_TOMBSTONE_OCCUPANCY_THRESH)
        return TMK_COMPACT_TOMBSTONE;

    /* If many generations have passed, use leveled */
    if (ctx->global_generation > TMK_GENERATION_BOUNDARY)
        return TMK_COMPACT_LEVELED;

    /* If retention threshold met, use time-window */
    if (ctx->global_epoch > TMK_RETENTION_EPOCH_THRESHOLD * 2)
        return TMK_COMPACT_TIME_WINDOW;

    /* Default: size-tiered */
    return TMK_COMPACT_SIZE_TIERED;
}

int tmk_compact_evaluate_candidates(tmk_context_t *ctx,
                                    tmk_compact_candidate_t *out,
                                    int max_candidates) {
    if (!out || max_candidates <= 0) return TMK_ERR_SHORT_BUF;

    int found = 0;
    tmk_compact_strategy_t strategy = tmk_compact_select_strategy(ctx);

    /* Scan segment pairs */
    for (int i = 0; i < TMK_MAX_SEGMENTS && found < max_candidates; i++) {
        if (!ctx->segments[i].occupied) continue;
        if (ctx->segments[i].phase != TMK_PHASE_ACTIVE) continue;

        float score = 0.0f;

        switch (strategy) {
        case TMK_COMPACT_SIZE_TIERED:
            /* Find best pair */
            for (int j = i + 1; j < TMK_MAX_SEGMENTS; j++) {
                if (!ctx->segments[j].occupied) continue;
                if (ctx->segments[j].phase != TMK_PHASE_ACTIVE) continue;
                if (ctx->segments[j].partition_id != ctx->segments[i].partition_id)
                    continue;

                float pair_score;
                tmk_compact_score_size_tiered(ctx, ctx->segments[i].id,
                                              ctx->segments[j].id, &pair_score);
                if (pair_score > score) {
                    score = pair_score;
                    out[found].candidate_a = ctx->segments[i].id;
                    out[found].candidate_b = ctx->segments[j].id;
                }
            }
            break;

        case TMK_COMPACT_LEVELED:
            tmk_compact_score_leveled(ctx, ctx->segments[i].id, &score);
            out[found].candidate_a = ctx->segments[i].id;
            out[found].candidate_b = 0;
            break;

        case TMK_COMPACT_TIME_WINDOW:
            tmk_compact_score_time_window(ctx, ctx->segments[i].id, &score);
            out[found].candidate_a = ctx->segments[i].id;
            out[found].candidate_b = 0;
            break;

        case TMK_COMPACT_TOMBSTONE:
            tmk_compact_score_tombstone(ctx, ctx->segments[i].id, &score);
            out[found].candidate_a = ctx->segments[i].id;
            out[found].candidate_b = 0;
            break;
        }

        if (score > 0.01f) {
            out[found].strategy = strategy;
            out[found].score = score;
            out[found].estimated_savings = 0;
            out[found].epoch_range = ctx->global_epoch - ctx->segments[i].create_epoch;
            found++;
        }
    }

    return found;
}
