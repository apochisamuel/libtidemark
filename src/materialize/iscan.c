/*
 * libtidemark — index scan materialization
 *
 * BUG SURFACE (index_tombstone_uaf): Cursor attached_index may
 * reference a freed tombstoned index node.
 */
#include "../internal/context.h"
#include "iscan.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_materialize_index_scan(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    uint16_t cursor_id = frame->header.route_id;
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    if (cur->attached_index) {
        tmk_index_node_t *idx = cur->attached_index;
        if (idx->active && idx->segment_ref) {
            tmk_segment_t *seg = idx->segment_ref;
            if (seg->occupied && seg->data && seg->used > 0) {
                volatile uint8_t check = seg->data[0];
                (void)check;
            }
        }
    }

    return TMK_OK;
}
