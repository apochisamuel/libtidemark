/*
 * libtidemark — range read materialization
 *
 * Reads a range from a segment using index-stored capacity.
 *
 * BUG SURFACE (compact_merge_oob): Index stored capacity may be
 * stale after segment merging.
 */
#include "../internal/context.h"
#include "range.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_materialize_read_range(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;

    uint16_t idx_id = frame->header.route_id;
    tmk_index_node_t *idx = tmk_find_index_node(ctx, idx_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;
    if (!idx->segment_ref) return TMK_ERR_NOT_FOUND;

    /* Read using index-stored capacity */
    tmk_segment_t *seg = idx->segment_ref;
    if (!seg->occupied || !seg->data) return TMK_ERR_NOT_FOUND;

    size_t read_limit = idx->stored_capacity;
    if (read_limit > 0) {
        size_t actual = (read_limit < seg->capacity) ? read_limit : seg->capacity;
        if (actual > 0) {
            volatile uint8_t check = seg->data[actual - 1];
            (void)check;
        }
    }

    return TMK_OK;
}
