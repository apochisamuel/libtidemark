/*
 * libtidemark — cross-partition cursor advance
 *
 * Advances a cursor through a cross-partition index.  Accesses
 * partition metadata through the index entry's partition_ref.
 *
 * BUG SURFACE (partition_merge_cursor): If partitions were merged,
 * the index entry's partition_ref is stale.
 */
#include "../internal/context.h"
#include "cross.h"
#include "../../include/tidemark/error.h"

int tmk_cursor_advance_cross(tmk_context_t *ctx, uint16_t cursor_id) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;
    if (!cur->attached_index) return TMK_ERR_NOT_FOUND;

    tmk_index_node_t *idx = cur->attached_index;
    if (!idx->active) return TMK_ERR_NOT_ACTIVE;

    /* Access partition ref for cross-partition traversal */
    if (idx->partition_ref) {
        if (!idx->partition_ref->active) return TMK_ERR_NOT_ACTIVE;
        cur->position++;
    }

    return TMK_OK;
}
