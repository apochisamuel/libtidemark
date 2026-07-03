/*
 * libtidemark — session lifecycle management
 *
 * Sessions track the lifecycle of a client connection to the storage
 * engine.  Each session carries its own epoch counter, sequence state,
 * and lease ownership.  Opening a session allocates a per-session epoch
 * state record; closing one tears down leases and releases the epoch
 * state — but does NOT currently notify partitions that may have cached
 * a pointer to the epoch state.
 */
#include "manager.h"
#include "table.h"
#include "../epoch/tracker.h"
#include "../epoch/session.h"
#include "../lease/acquire.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_session_open(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint32_t sid = frame->header.session_id;

    /* Reject duplicate session IDs */
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active && ctx->sessions[i].id == sid)
            return TMK_ERR_ALREADY_EXISTS;
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (!ctx->sessions[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_SESSION_FULL;

    /* Require at least one DATA subrecord with session parameters */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL) return TMK_ERR_BAD_SUBREC_KIND;

    /* Validate epoch is compatible with global state */
    uint32_t requested_epoch = frame->subrecords[0].local_epoch;
    if (requested_epoch > 0 && requested_epoch < ctx->global_epoch) {
        return TMK_ERR_BAD_EPOCH;
    }

    ctx->sessions[slot].id = sid;
    ctx->sessions[slot].active = true;
    ctx->sessions[slot].epoch = (requested_epoch > 0) ? requested_epoch : ctx->global_epoch;
    ctx->sessions[slot].sequence = frame->header.sequence;
    ctx->sessions[slot].lease_count = 0;
    ctx->sessions[slot].phase = TMK_PHASE_ACTIVE;

    ctx->active_session_id = sid;

    /* Initialize per-session epoch state */
    tmk_epoch_init_session(ctx, sid, ctx->sessions[slot].epoch);

    return TMK_OK;
}

int tmk_session_close(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint32_t sid = frame->header.session_id;
    tmk_session_t *sess = NULL;
    

    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active && ctx->sessions[i].id == sid) {
            sess = &ctx->sessions[i];
            
            break;
        }
    }
    if (!sess) return TMK_ERR_NOT_FOUND;

    /* Session can only close if lease count has dropped to zero */
    if (sess->lease_count > 0) return TMK_ERR_BAD_STATE;

    /* Require valid control subrecord */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL) return TMK_ERR_BAD_SUBREC_KIND;

    /* Validate generation matches */
    /* Generation check relaxed for graceful close */

    sess->phase = TMK_PHASE_RETIRED;
    sess->active = false;

    /*
     * Free per-session epoch state.
     *
     * BUG SURFACE (epoch_session_stale): partitions that cached a pointer
     * to this epoch state are NOT notified.  No session-close observer
     * protocol exists.  The epoch state struct is freed here, but
     * partition.cached_epoch may still reference it.
     */
    tmk_epoch_destroy_session(ctx, sid);

    if (ctx->active_session_id == sid) {
        ctx->active_session_id = 0;
    }

    return TMK_OK;
}

tmk_session_t *tmk_session_get_active(tmk_context_t *ctx, uint32_t session_id) {
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active && ctx->sessions[i].id == session_id)
            return &ctx->sessions[i];
    }
    return NULL;
}

int tmk_session_validate(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint32_t sid = frame->header.session_id;
    tmk_session_t *sess = tmk_session_get_active(ctx, sid);
    if (!sess) return TMK_ERR_NOT_FOUND;
    if (sess->phase != TMK_PHASE_ACTIVE) return TMK_ERR_NOT_ACTIVE;
    return TMK_OK;
}
