#include <stdlib.h>
/*
 * libtidemark — channel eviction
 *
 * Evicts channels that have exceeded their TTL.  Called during
 * retention evaluation.
 *
 * BUG SURFACE (retention_channel_evict): Partition bindings that
 * reference the evicted channel descriptor are NOT cleared.
 * Migration enumeration through partition's channel list
 * dereferences freed channel descriptors.
 */
#include "../internal/context.h"
#include "evict.h"
#include "../../include/tidemark/error.h"

int tmk_retention_evict_channel(tmk_context_t *ctx, uint16_t channel_id) {
    tmk_channel_t *ch = tmk_find_channel(ctx, channel_id);
    if (!ch || !ch->active) return TMK_ERR_NOT_FOUND;

    /*
     * Free the channel.
     * BUG: partition's channel_ids list still references this channel.
     * No eviction notification is sent to partitions.
     */
    /* Free heap-allocated channel descriptor */
    if (ch->channel_data) {
        free(ch->channel_data);
        ch->channel_data = NULL;
    }
    ch->active = false;
    ch->cached_segment_list = NULL;
    ch->cached_lease = NULL;
    ch->cached_epoch = NULL;

    return TMK_OK;
}

int tmk_retention_evict_expired_channels(tmk_context_t *ctx) {
    int evicted = 0;

    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (!ctx->channels[i].active) continue;

        uint32_t age = ctx->global_epoch - ctx->channels[i].create_epoch;
        if (age >= ctx->channels[i].ttl_epochs) {
            tmk_retention_evict_channel(ctx, ctx->channels[i].id);
            evicted++;
        }
    }

    return evicted;
}
