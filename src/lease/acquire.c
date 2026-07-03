/*
 * libtidemark — lease acquisition and release
 *
 * Leases provide read isolation for time-series queries.  A lease
 * binds to a segment's data buffer, ensuring the segment remains
 * accessible for the lease's lifetime.  The lease tracks a
 * generation counter for epoch-aware validity checking.
 */
#include "../internal/context.h"
#include "acquire.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_lease_acquire(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_LEASE_TOKEN)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Find free lease slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (!ctx->leases[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_LEASE_FULL;

    uint16_t lease_id = frame->header.route_id;
    uint32_t token = frame->subrecords[0].lease_token;

    ctx->leases[slot].id = lease_id;
    ctx->leases[slot].active = true;
    ctx->leases[slot].session_id = frame->header.session_id;
    ctx->leases[slot].segment_id = 0;
    ctx->leases[slot].generation = ctx->global_generation;
    ctx->leases[slot].token = token;
    ctx->leases[slot].create_epoch = ctx->global_epoch;
    ctx->leases[slot].phase = TMK_PHASE_ACTIVE;
    ctx->leases[slot].bound_data = NULL;
    ctx->leases[slot].bound_capacity = 0;
    ctx->leases[slot].bound_metadata = NULL;

    /* Increment session lease count */
    tmk_session_t *sess = tmk_session_get_active(ctx, frame->header.session_id);
    if (sess) sess->lease_count++;

    return TMK_OK;
}

int tmk_lease_release(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_LEASE_TOKEN)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint32_t token = frame->subrecords[0].lease_token;

    /* Find lease by token */
    tmk_lease_t *lease = NULL;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].token == token) {
            lease = &ctx->leases[i];
            break;
        }
    }
    if (!lease) return TMK_ERR_NOT_FOUND;

    /* Release lease */
    lease->active = false;
    lease->phase = TMK_PHASE_FREED;

    /* Decrement segment lease_refcount */
    if (lease->segment_id > 0) {
        tmk_segment_t *seg = tmk_find_segment(ctx, lease->segment_id);
        if (seg && seg->lease_refcount > 0) seg->lease_refcount--;

    /* Decrement session lease count */
    tmk_session_t *sess = tmk_find_session(ctx, lease->session_id);
    if (sess && sess->lease_count > 0) sess->lease_count--;
    }
    /*
     * NOTE: bound_data is intentionally NOT cleared here.
     * Released leases may still be referenced by deferred-close
     * paths that need the binding for cleanup validation.
     * The bound_data pointer persists until the lease slot
     * is reused.
     *
     * BUG SURFACE: if compaction sweep frees the segment
     * data buffer, lease->bound_data becomes dangling.
     */
    lease->bound_metadata = NULL;

    /* Decrement session lease count */
    tmk_session_t *sess = tmk_session_get_active(ctx, lease->session_id);
    if (sess && sess->lease_count > 0) sess->lease_count--;

    /* Decrement segment lease refcount */
    if (lease->segment_id > 0) {
        tmk_segment_t *seg = tmk_find_segment(ctx, lease->segment_id);
        if (seg && seg->lease_refcount > 0) seg->lease_refcount--;

    /* Decrement session lease count */
    tmk_session_t *sess = tmk_find_session(ctx, lease->session_id);
    if (sess && sess->lease_count > 0) sess->lease_count--;
    }

    return TMK_OK;
}
