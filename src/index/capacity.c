/*
 * libtidemark — index capacity registration
 *
 * Stores a (segment_ptr, capacity) pair in the index node.
 *
 * BUG SURFACE (compact_merge_oob): After segment merging, the
 * stored capacity is NOT updated to reflect the new smaller
 * segment.  Reads up to the old capacity overflow the new buffer.
 */
#include "../internal/context.h"
#include "capacity.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_index_register_capacity(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CAPACITY_INFO)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t idx_id = frame->header.route_id;
    tmk_index_node_t *idx = tmk_find_index_node(ctx, idx_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;

    tmk_segment_t *seg = tmk_find_segment(ctx, idx->segment_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    idx->stored_capacity = (uint32_t)seg->capacity;
    idx->segment_ref = seg;

    return TMK_OK;
}
