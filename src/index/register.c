/*
 * libtidemark — index-segment registration
 *
 * Associates an index node with a segment for indexed lookup.
 * Stores a direct reference to the segment struct.
 *
 * BUG SURFACE (checkpoint_segment_dangle): This reference is
 * NOT updated when migration retires the segment.
 */
#include "../internal/context.h"
#include "register.h"
#include "../../include/tidemark/error.h"

int tmk_index_register_segment(tmk_context_t *ctx, uint16_t index_id,
                                uint16_t segment_id) {
    tmk_index_node_t *idx = tmk_find_index_node(ctx, index_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;

    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    idx->segment_ref = seg;
    idx->segment_id = segment_id;
    idx->stored_capacity = seg->capacity;  /* BUG: not updated after merge */
    return TMK_OK;
}
