/*
 * libtidemark — index-cursor binding
 *
 * Stores a pointer to a cursor's state in the index node for
 * fast index-cursor coordination.
 *
 * BUG SURFACE (cursor_gen_realloc): If the cursor table is
 * compacted, this pointer becomes stale.
 */
#include "../internal/context.h"
#include "bind.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_index_bind_cursor(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_INDEX_ENTRY)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t idx_id = frame->header.route_id;
    uint16_t cursor_id = frame->header.channel_id; /* reuse */

    tmk_index_node_t *idx = tmk_find_index_node(ctx, idx_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;

    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    idx->cursor_ref = cur;
    return TMK_OK;
}
