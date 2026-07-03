/*
 * libtidemark — retention policy enforcer
 *
 * Evaluates time-based retention policies.  When the global epoch
 * exceeds a segment's create_epoch + TTL, the segment becomes
 * eligible for retirement and eventual compaction.
 *
 * Retention evaluation is triggered by:
 * 1. Explicit retention_trigger frames
 * 2. Epoch cascade (generation boundary crossing)
 */
#include "enforcer.h"
#include "watermark.h"
#include "evict.h"
#include "../compact/revoke.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_retention_configure(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_RETENTION_POLICY)
        return TMK_ERR_BAD_SUBREC_KIND;

    ctx->retention.watermark_epoch = frame->subrecords[0].local_epoch;
    ctx->retention.ttl_epochs = frame->subrecords[0].generation;
    if (ctx->retention.ttl_epochs == 0)
        ctx->retention.ttl_epochs = TMK_CHANNEL_TTL_EPOCHS;
    ctx->retention.cascade_enabled = (frame->header.flags & TMK_FLAG_COMPACT) != 0;
    ctx->retention.configured = true;

    return TMK_OK;
}

int tmk_retention_trigger(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    return tmk_retention_evaluate_watermarks(ctx);
}

int tmk_retention_evaluate_watermarks(tmk_context_t *ctx) {
    int retired = 0;

    /* Evaluate segment retention */
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (!ctx->segments[i].occupied) continue;
        if (ctx->segments[i].phase != TMK_PHASE_ACTIVE &&
            ctx->segments[i].phase != TMK_PHASE_SEALED) continue;

        uint32_t age = ctx->global_epoch - ctx->segments[i].create_epoch;
        if (age >= TMK_RETENTION_EPOCH_THRESHOLD) {
            /* Check if segment has no active lease refcount */
            if (ctx->segments[i].lease_refcount == 0) {
                ctx->segments[i].phase = TMK_PHASE_RETIRING;
                ctx->segments[i].tombstone = true;
                ctx->tombstone_count++;
                retired++;
            }
        }
    }

    /* If cascade is enabled, also revoke expired leases */
    if (ctx->retention.cascade_enabled) {
        tmk_compact_revoke_expired(ctx);
    }

    /* Evaluate channel retention (TTL-based) */
    tmk_retention_evict_expired_channels(ctx);

    if (retired == 0) return TMK_ERR_RETENTION_SKIP;
    return retired;
}
