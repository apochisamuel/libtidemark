/*
 * libtidemark — channel epoch validation
 *
 * During frame dispatch, the channel validates the frame's epoch
 * against the partition's cached epoch state for fast rejection of
 * stale frames.
 *
 * BUG SURFACE (epoch_session_stale): The partition's cached epoch
 * state pointer may reference a freed session epoch state if the
 * owning session was closed.  This function reads through that
 * potentially dangling pointer.
 */
#include "../internal/context.h"
#include "validate.h"
#include "router.h"
#include "../../include/tidemark/error.h"

int tmk_channel_validate_epoch(tmk_context_t *ctx, uint16_t channel_id,
                               const tmk_parsed_frame_t *frame) {
    tmk_channel_t *ch = tmk_find_channel(ctx, channel_id);
    if (!ch || !ch->active) return TMK_ERR_NOT_FOUND;

    tmk_partition_t *part = tmk_find_partition(ctx, ch->bound_partition_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    /*
     * If partition has cached epoch state, use it for fast validation.
     * This cached pointer was set by partition_cache_epoch and may be
     * stale if the session that owned it has closed.
     */
    if (part->cached_epoch) {
        if (frame->subrecords[0].local_epoch < part->cached_epoch->global_epoch)
            return TMK_ERR_BAD_EPOCH;
    }

    return TMK_OK;
}
