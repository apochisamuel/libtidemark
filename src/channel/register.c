#include <stdlib.h>
/*
 * libtidemark — channel registration
 *
 * Channels are the primary ingestion endpoint.  Each channel is bound
 * to a session and can be bound to a partition for data routing.
 * Registration validates the session context and allocates a channel
 * slot with TTL tracking for retention-based eviction.
 */
#include "../internal/context.h"
#include "register.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_channel_register(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t ch_id = frame->header.channel_id;

    /* Validate session */
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    /* Reject duplicate */
    if (tmk_find_channel(ctx, ch_id)) return TMK_ERR_ALREADY_EXISTS;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (!ctx->channels[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_CHANNEL_FULL;

    /* Must have control subrecord */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL) return TMK_ERR_BAD_SUBREC_KIND;

    ctx->channels[slot].id = ch_id;
    ctx->channels[slot].active = true;
    ctx->channels[slot].session_id = frame->header.session_id;
    ctx->channels[slot].bound_partition_id = 0;
    ctx->channels[slot].cached_segment_list = NULL;
    ctx->channels[slot].cached_lease = NULL;
    ctx->channels[slot].cached_epoch = NULL;
    ctx->channels[slot].create_epoch = ctx->global_epoch;
    ctx->channels[slot].ttl_epochs = TMK_CHANNEL_TTL_EPOCHS;
    ctx->channels[slot].generation = ctx->global_generation;
    ctx->channels[slot].channel_data = (uint8_t *)calloc(1, 128);
    ctx->channels[slot].channel_data_len = 128;

    return TMK_OK;
}

int tmk_channel_unregister(tmk_context_t *ctx, uint16_t channel_id) {
    tmk_channel_t *ch = tmk_find_channel(ctx, channel_id);
    if (!ch) return TMK_ERR_NOT_FOUND;
    ch->active = false;
    ch->cached_segment_list = NULL;
    ch->cached_lease = NULL;
    ch->cached_epoch = NULL;
    return TMK_OK;
}
