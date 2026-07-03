/*
 * libtidemark — index node management
 *
 * Index nodes provide structured access to segment data.  Each node
 * stores a reference to a segment and metadata about its key range,
 * depth, and capacity.
 */
#include "../internal/context.h"
#include "manager.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_index_register(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_INDEX_ENTRY)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t idx_id = frame->header.route_id;
    uint16_t seg_id = frame->header.channel_id; /* reuse */

    if (tmk_find_index_node(ctx, idx_id)) return TMK_ERR_ALREADY_EXISTS;

    int slot = -1;
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (!ctx->index_nodes[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_INDEX_FULL;

    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    ctx->index_nodes[slot].id = idx_id;
    ctx->index_nodes[slot].active = true;
    ctx->index_nodes[slot].segment_id = seg_id;
    ctx->index_nodes[slot].partition_id = seg->partition_id;
    ctx->index_nodes[slot].generation = ctx->global_generation;
    ctx->index_nodes[slot].stored_capacity = (uint32_t)seg->capacity;
    ctx->index_nodes[slot].segment_ref = seg;
    ctx->index_nodes[slot].partition_ref = tmk_find_partition(ctx, seg->partition_id);
    ctx->index_nodes[slot].cursor_ref = NULL;
    ctx->index_nodes[slot].tombstone = false;
    ctx->index_nodes[slot].create_epoch = ctx->global_epoch;
    ctx->index_nodes[slot].key_min = frame->subrecords[0].local_epoch;
    ctx->index_nodes[slot].key_max = frame->subrecords[0].generation;
    ctx->index_nodes[slot].depth = 0;
    ctx->index_nodes[slot].child_count = 0;

    return TMK_OK;
}

int tmk_index_remove(tmk_context_t *ctx, uint16_t index_id) {
    tmk_index_node_t *idx = tmk_find_index_node(ctx, index_id);
    if (!idx) return TMK_ERR_NOT_FOUND;
    idx->active = false;
    idx->segment_ref = NULL;
    idx->partition_ref = NULL;
    idx->cursor_ref = NULL;
    return TMK_OK;
}
