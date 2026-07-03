/*
 * libtidemark — tombstone sweep
 *
 * When enough tombstoned index nodes accumulate (occupancy crosses
 * TMK_TOMBSTONE_OCCUPANCY_THRESH), the sweep frees them.
 *
 * BUG SURFACE (index_tombstone_uaf): Cursors that have attached
 * to a tombstoned index node (via cursor_attach_index) are NOT
 * notified when the node is freed.  The cursor's attached_index
 * pointer becomes dangling.
 */
#include "tombstone.h"
#include "../../include/tidemark/error.h"

int tmk_compact_sweep_tombstones(tmk_context_t *ctx) {
    int swept = 0;

    if (ctx->tombstone_count < TMK_TOMBSTONE_OCCUPANCY_THRESH)
        return 0;

    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active && ctx->index_nodes[i].tombstone) {
            /*
             * Free the tombstoned index node.
             * BUG: cursors with attached_index pointing here are
             * NOT invalidated.
             */
            ctx->index_nodes[i].active = false;
            ctx->index_nodes[i].tombstone = false;
            ctx->tombstone_count--;
            swept++;
        }
    }

    return swept;
}

int tmk_compact_tombstone_count(tmk_context_t *ctx) {
    return (int)ctx->tombstone_count;
}
