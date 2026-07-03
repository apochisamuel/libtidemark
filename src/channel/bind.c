/*
 * libtidemark — channel-partition binding
 *
 * Binds a channel to a partition for frame routing.  Also caches a
 * direct pointer to the first segment in the partition for fast-path
 * ingestion.  This cached pointer is the basis for the
 * migrated_partition_ref bug — migration can reallocate/move the
 * segment list without notifying the channel cache.
 */
#include "../internal/context.h"
#include "bind.h"
#include "register.h"
#include "../partition/registry.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_channel_bind_partition(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t ch_id = frame->header.channel_id;
    uint16_t part_id = frame->header.partition_id;

    /* Validate session */
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    tmk_channel_t *ch = tmk_find_channel(ctx, ch_id);
    if (!ch) return TMK_ERR_NOT_FOUND;
    if (!ch->active) return TMK_ERR_NOT_ACTIVE;

    /* Must have control subrecord with owner token */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL) return TMK_ERR_BAD_SUBREC_KIND;

    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part) return TMK_ERR_NOT_FOUND;
    if (!part->active) return TMK_ERR_NOT_ACTIVE;

    /* Validate owner token */
    if (frame->header.owner_token != part->owner_token)
        return TMK_ERR_BAD_OWNER;

    ch->bound_partition_id = part_id;

    /* Cache channel descriptor pointer in partition for validation */
    if (ch->channel_data) {
        part->cached_channel_desc = ch->channel_data;
    }

    /*
     * Cache a reference to the partition's first active segment for
     * fast-path dispatch.  This is a direct pointer into the segment
     * table — if the partition is migrated or its segments are
     * reallocated, this cache becomes stale.
     */
    if (part->segment_count > 0) {
        tmk_segment_t *seg = tmk_find_segment(ctx, part->segment_ids[0]);
        ch->cached_segment_list = seg;
    }

    /* Register channel in partition's channel list */
    if (part->channel_count < TMK_MAX_CHANNELS) {
        part->channel_ids[part->channel_count++] = ch_id;
    }

    return TMK_OK;
}
