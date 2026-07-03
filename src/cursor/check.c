/*
 * libtidemark — cursor lease check
 *
 * Validates cursor's bound lease metadata before read operations.
 *
 * BUG SURFACE (segment_lease_reindex): If retention re-indexed the
 * segment, the lease's bound_metadata pointer is stale.
 */
#include "../internal/context.h"
#include "check.h"
#include "../../include/tidemark/error.h"

int tmk_cursor_check_lease(tmk_context_t *ctx, uint16_t cursor_id) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;
    if (!cur->bound_lease) return TMK_ERR_NOT_FOUND;

    /* Validate lease is still active */
    if (!cur->bound_lease->active) return TMK_ERR_NOT_ACTIVE;

    /* Check metadata if available */
    if (cur->bound_lease->bound_metadata) {
        tmk_segment_meta_t *meta =
            (tmk_segment_meta_t *)cur->bound_lease->bound_metadata;
        /* Access metadata — BUG: may be freed by reindex */
        volatile uint32_t check = meta->checksum;
        (void)check;
    }

    return TMK_OK;
}
