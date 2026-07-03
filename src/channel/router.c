/*
 * libtidemark — channel router
 *
 * Routes incoming frames to their target partition based on channel
 * bindings.  The router maintains fast-path caches for frequently
 * accessed partitions and segments.
 */
#include "../internal/context.h"
#include "router.h"
#include "bind.h"
#include "register.h"
#include "../partition/registry.h"
#include "../../include/tidemark/error.h"

int tmk_channel_route_frame(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t ch_id = frame->header.channel_id;
    tmk_channel_t *ch = tmk_find_channel(ctx, ch_id);
    if (!ch) return TMK_ERR_NOT_FOUND;
    if (!ch->active) return TMK_ERR_NOT_ACTIVE;

    /* Validate session ownership */
    if (ch->session_id != frame->header.session_id)
        return TMK_ERR_BAD_OWNER;

    /* Resolve target partition */
    if (ch->bound_partition_id == 0) return TMK_ERR_NOT_FOUND;
    tmk_partition_t *part = tmk_find_partition(ctx, ch->bound_partition_id);
    if (!part) return TMK_ERR_NOT_FOUND;
    if (!part->active) return TMK_ERR_NOT_ACTIVE;

    return TMK_OK;
}

tmk_partition_t *tmk_channel_resolve_partition(tmk_context_t *ctx, uint16_t channel_id) {
    tmk_channel_t *ch = tmk_find_channel(ctx, channel_id);
    if (!ch || !ch->active) return NULL;
    if (ch->bound_partition_id == 0) return NULL;
    return tmk_find_partition(ctx, ch->bound_partition_id);
}
