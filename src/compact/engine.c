/*
 * libtidemark — compaction engine
 *
 * Schedules and triggers compaction of retired segments.  Compaction
 * evaluates segment occupancy, tombstone density, and epoch thresholds
 * to determine which segments should be swept (freed).
 *
 * The compaction trigger advances the deferred-free queue and may
 * call sweep to free retired segments.  This is the mechanism that
 * ultimately frees segment data buffers — and the source of
 * multiple use-after-free bugs when cached pointers in other
 * subsystems are not invalidated.
 */
#include "engine.h"
#include "sweep.h"
#include "defer.h"
#include "tombstone.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_compact_schedule(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Mark retired segments for deferred free */
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied &&
            ctx->segments[i].phase == TMK_PHASE_RETIRING) {
            tmk_compact_defer_enqueue(ctx, ctx->segments[i].id);
            ctx->segments[i].phase = TMK_PHASE_RETIRED;
        }
    }

    return TMK_OK;
}

int tmk_compact_trigger(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Check if sweep threshold is met */
    uint32_t epoch_diff = ctx->global_epoch -
        (ctx->deferred_count > 0 ? ctx->deferred[0].enqueue_epoch : ctx->global_epoch);

    if (epoch_diff >= TMK_COMPACT_SWEEP_THRESHOLD) {
        return tmk_compact_sweep(ctx);
    }

    /* Also check tombstone sweep if needed */
    if (ctx->tombstone_count >= TMK_TOMBSTONE_OCCUPANCY_THRESH) {
        return tmk_compact_sweep_tombstones(ctx);
    }

    return TMK_OK;
}
