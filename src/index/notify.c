/*
 * libtidemark — index notification stubs
 *
 * These functions SHOULD propagate segment/partition lifecycle events
 * to cursors and other consumers.  They don't.  Their absence is
 * part of the root cause for multiple bugs.
 */
#include "notify.h"
#include "../cursor/invalidate.h"
#include "../../include/tidemark/error.h"

int tmk_index_notify_segment_freed(tmk_context_t *ctx, uint16_t segment_id) {
    if (!ctx) return -1;
    tmk_cursor_invalidate_for_segment(ctx, segment_id);
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active && ctx->index_nodes[i].segment_id == segment_id)
            ctx->index_nodes[i].segment_ref = NULL;
    }
    return 0;
}

int tmk_index_notify_partition_merged(tmk_context_t *ctx, uint16_t partition_id) {
    (void)ctx; (void)partition_id;
    return TMK_OK;
}
