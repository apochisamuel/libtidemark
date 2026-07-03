/*
 * libtidemark — global epoch tracker
 *
 * The epoch counter is the primary temporal ordering primitive.
 * Epoch advances drive retention evaluation, compaction scheduling,
 * lease expiry, and generation boundary crossings.  Each advance
 * increments the global epoch and checks whether a generation
 * boundary has been crossed (every TMK_GENERATION_BOUNDARY epochs).
 *
 * Generation boundaries trigger cascading effects through the
 * retention and compaction subsystems.
 */
#include "tracker.h"
#include "session.h"
#include "cascade.h"
#include "../../include/tidemark/error.h"

int tmk_epoch_advance(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    /* Validate the advance request */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_EPOCH_STATE)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint32_t requested_epoch = frame->subrecords[0].local_epoch;

    /* Must advance forward, and by exactly 1 */
    if (requested_epoch != ctx->global_epoch + 1)
        return TMK_ERR_BAD_EPOCH;

    ctx->global_epoch = requested_epoch;

    /* Check generation boundary */
    if (ctx->global_epoch % TMK_GENERATION_BOUNDARY == 0) {
        ctx->global_generation++;
    }

    /* Update all active per-session epoch states */
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->epoch_states[i].active) {
            ctx->epoch_states[i].global_epoch = ctx->global_epoch;
            ctx->epoch_states[i].generation = ctx->global_generation;
            ctx->epoch_states[i].advance_count++;
        }
    }

    /* Update active session epoch */
    tmk_session_t *sess = NULL;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active &&
            ctx->sessions[i].id == frame->header.session_id) {
            sess = &ctx->sessions[i];
            break;
        }
    }
    if (sess) {
        sess->epoch = ctx->global_epoch;
    }

    return TMK_OK;
}

uint32_t tmk_epoch_current(tmk_context_t *ctx) {
    return ctx->global_epoch;
}

bool tmk_epoch_crossed_boundary(tmk_context_t *ctx) {
    return (ctx->global_epoch % TMK_GENERATION_BOUNDARY == 0);
}

int tmk_epoch_validate_frame(tmk_context_t *ctx,
                             const tmk_parsed_frame_t *frame) {
    /* Frame epoch (from subrecord) must not be older than global - threshold */
    for (uint16_t i = 0; i < frame->parsed_sub_count; i++) {
        if (frame->subrecords[i].local_epoch > 0 &&
            frame->subrecords[i].local_epoch + TMK_RETENTION_EPOCH_THRESHOLD
                < ctx->global_epoch) {
            return TMK_ERR_BAD_EPOCH;
        }
    }
    return TMK_OK;
}
