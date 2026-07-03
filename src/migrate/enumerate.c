/*
 * libtidemark — migration channel enumeration
 *
 * Enumerates channels bound to a partition for migration preparation.
 * Iterates through the partition's channel_ids list and accesses
 * each channel descriptor.
 *
 * BUG SURFACE (retention_channel_evict): If retention has evicted
 * a channel from the channel table but the partition's channel_ids
 * list still references it, the lookup returns a freed channel
 * descriptor.
 */
#include "../internal/context.h"
#include "enumerate.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_migrate_enumerate_channels(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    uint16_t part_id = frame->header.partition_id;
    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    /* Enumerate channels */
    int count = 0;
    for (uint16_t i = 0; i < part->channel_count; i++) {
        tmk_channel_t *ch = tmk_find_channel(ctx, part->channel_ids[i]);
        if (ch && ch->active) {
            count++;
        }
    }

    return count;
}
