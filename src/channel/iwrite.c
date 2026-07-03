/*
 * libtidemark — channel index-assisted write
 *
 * Performs data ingestion through the index-assisted write path.
 * Looks up the index entry for the target segment and uses the
 * index-cached cursor reference for fast positioning.
 *
 * BUG SURFACE (cursor_gen_realloc): If the cursor table was
 * compacted/reallocated, the index's cached cursor pointer is stale.
 */
#include "../internal/context.h"
#include "iwrite.h"
#include "router.h"
#include "../index/manager.h"
#include "../segment/store.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_channel_index_write(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t ch_id = frame->header.channel_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    tmk_channel_t *ch = tmk_find_channel(ctx, ch_id);
    if (!ch || !ch->active) return TMK_ERR_NOT_FOUND;

    /* Need data subrecord */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_DATA) return TMK_ERR_BAD_SUBREC_KIND;

    /* Find index entry for the route/segment */
    uint16_t route = frame->header.route_id;
    tmk_index_node_t *idx = NULL;
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active &&
            ctx->index_nodes[i].segment_id == route) {
            idx = &ctx->index_nodes[i];
            break;
        }
    }
    if (!idx) return TMK_ERR_NOT_FOUND;

    /*
     * If index has a cached cursor reference, use it for positioning.
     * BUG: cursor_ref may point to freed memory if cursor table
     * was compacted.
     */
    if (idx->cursor_ref) {
        if (!idx->cursor_ref->active) return TMK_ERR_NOT_ACTIVE;
    }

    /* Write data to segment */
    tmk_segment_t *seg = tmk_find_segment(ctx, route);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;
    if (seg->phase != TMK_PHASE_ACTIVE) return TMK_ERR_BAD_STATE;

    const tmk_subrecord_t *sub = &frame->subrecords[0];
    if (sub->payload && sub->length > 0) {
        size_t to_write = sub->length;
        if (seg->used + to_write > seg->capacity) {
            to_write = seg->capacity - seg->used;
        }
        if (to_write > 0 && seg->data) {
            memcpy(seg->data + seg->used, sub->payload, to_write);
            seg->used += to_write;
        }
    }

    return TMK_OK;
}
