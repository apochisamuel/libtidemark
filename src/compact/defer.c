/*
 * libtidemark — deferred free queue
 *
 * Segments are not freed immediately when retired.  Instead they
 * are placed on a deferred queue with an enqueue epoch timestamp.
 * After enough epoch advances (TMK_DEFERRED_DRAIN_THRESHOLD), the
 * deferred entries are eligible for drain.
 *
 * BUG SURFACE (deferred_segment_free): The drain checks only the
 * compact_refcount.  Lease bindings (lease_refcount) are not
 * consulted because no cross-subsystem refcount reconciliation
 * protocol exists.
 */
#include "defer.h"
#include "sweep.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_compact_defer_enqueue(tmk_context_t *ctx, uint16_t segment_id) {
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_DEFERRED; i++) {
        if (!ctx->deferred[i].occupied) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_DEFERRED_FULL;

    ctx->deferred[slot].segment_id = segment_id;
    ctx->deferred[slot].enqueue_epoch = ctx->global_epoch;
    ctx->deferred[slot].compact_refcount = 0;
    ctx->deferred[slot].occupied = true;
    ctx->deferred_count++;

    return TMK_OK;
}

int tmk_compact_defer_drain(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Check if drain threshold met */
    if (ctx->deferred_count < TMK_DEFERRED_DRAIN_THRESHOLD)
        return TMK_OK;

    /* Trigger sweep which drains eligible entries */
    return tmk_compact_sweep(ctx);
}

int tmk_compact_defer_count(tmk_context_t *ctx) {
    return (int)ctx->deferred_count;
}
