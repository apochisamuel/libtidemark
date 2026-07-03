/*
 * libtidemark — index-resolved materialization
 *
 * BUG SURFACE (checkpoint_segment_dangle): Index may reference
 * a freed segment.
 */
#include "../internal/context.h"
#include "resolve.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_materialize_index_resolve(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    uint16_t idx_id = frame->header.route_id;
    tmk_index_node_t *idx = tmk_find_index_node(ctx, idx_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;

    if (idx->segment_ref) {
        tmk_segment_t *seg = idx->segment_ref;
        if (seg->occupied && seg->data && seg->used > 0) {
            volatile uint8_t check = seg->data[0];
            (void)check;
        }
    }

    return TMK_OK;
}
