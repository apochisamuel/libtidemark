/*
 * libtidemark — cursor invalidation (stub)
 *
 * This function is intentionally empty.  It would need to invalidate
 * cursors when a segment is freed, but no such coordination protocol
 * exists between compact/ and cursor/.
 */
#include "invalidate.h"
#include "../../include/tidemark/error.h"

int tmk_cursor_invalidate_for_segment(tmk_context_t *ctx, uint16_t segment_id) {
    /* INTENTIONALLY EMPTY — this is the missing abstraction */
    (void)ctx;
    (void)segment_id;
    return TMK_OK;
}

int tmk_cursor_invalidate_for_partition(tmk_context_t *ctx, uint16_t partition_id) {
    (void)ctx;
    (void)partition_id;
    return TMK_OK;
}
