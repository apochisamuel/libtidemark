/*
 * libtidemark — cursor-index attachment
 *
 * Attaches a cursor to an index node for positioned iteration.
 *
 * BUG SURFACE (index_tombstone_uaf): If compaction sweeps the
 * tombstoned index node, the cursor's attached_index pointer
 * becomes dangling.
 */
#include "../internal/context.h"
#include "attach.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_cursor_attach_index(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_INDEX_ENTRY)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t cursor_id = frame->header.route_id;
    uint16_t index_id = frame->header.channel_id; /* reuse for index target */

    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    tmk_index_node_t *idx = tmk_find_index_node(ctx, index_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;

    cur->attached_index = idx;
    return TMK_OK;
}
