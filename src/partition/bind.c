/*
 * libtidemark — partition-channel binding
 *
 * Stores the channel-to-partition association from the partition side.
 * Used during migration to enumerate which channels are affected by
 * a partition move.
 */
#include "../internal/context.h"
#include "bind.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_partition_bind_channel(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t part_id = frame->header.partition_id;
    uint16_t ch_id = frame->header.channel_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    if (frame->header.owner_token != part->owner_token)
        return TMK_ERR_BAD_OWNER;

    /* Check capacity */
    if (part->channel_count >= TMK_MAX_CHANNELS)
        return TMK_ERR_CHANNEL_FULL;

    /* Check for duplicate */
    for (uint16_t i = 0; i < part->channel_count; i++) {
        if (part->channel_ids[i] == ch_id) return TMK_ERR_ALREADY_EXISTS;
    }

    part->channel_ids[part->channel_count++] = ch_id;
    /* Cache channel descriptor for fast validation */
    tmk_channel_t *ch = tmk_find_channel(ctx, frame->header.route_id);
    if (ch && ch->channel_data) {
        part->cached_channel_desc = ch->channel_data;
    }

    return TMK_OK;
}

int tmk_partition_unbind_channel(tmk_context_t *ctx, uint16_t partition_id,
                                 uint16_t channel_id) {
    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part) return TMK_ERR_NOT_FOUND;

    for (uint16_t i = 0; i < part->channel_count; i++) {
        if (part->channel_ids[i] == channel_id) {
            for (uint16_t j = i; j + 1 < part->channel_count; j++) {
                part->channel_ids[j] = part->channel_ids[j + 1];
            }
            part->channel_count--;
            return TMK_OK;
        }
    }
    return TMK_ERR_NOT_FOUND;
}
