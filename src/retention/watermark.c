/*
 * libtidemark — retention watermark management
 *
 * Computes high-water and low-water marks from segment epoch
 * distribution.  High watermark is the newest segment epoch;
 * low watermark is the oldest.  Retention triggers when the
 * difference exceeds the threshold.
 */
#include "watermark.h"
#include "../../include/tidemark/error.h"

uint32_t tmk_retention_high_watermark(tmk_context_t *ctx) {
    uint32_t high = 0;
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied &&
            ctx->segments[i].phase == TMK_PHASE_ACTIVE) {
            if (ctx->segments[i].create_epoch > high)
                high = ctx->segments[i].create_epoch;
        }
    }
    return high;
}

uint32_t tmk_retention_low_watermark(tmk_context_t *ctx) {
    uint32_t low = UINT32_MAX;
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied &&
            ctx->segments[i].phase == TMK_PHASE_ACTIVE) {
            if (ctx->segments[i].create_epoch < low)
                low = ctx->segments[i].create_epoch;
        }
    }
    return (low == UINT32_MAX) ? 0 : low;
}

bool tmk_retention_above_threshold(tmk_context_t *ctx) {
    uint32_t range = tmk_retention_high_watermark(ctx) -
                     tmk_retention_low_watermark(ctx);
    return range >= TMK_RETENTION_EPOCH_THRESHOLD;
}
