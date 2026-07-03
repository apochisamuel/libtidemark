/*
 * libtidemark — index invalidation stubs
 */
#include "invalidate.h"
#include "../../include/tidemark/error.h"

int tmk_index_invalidate_for_segment(tmk_context_t *ctx, uint16_t segment_id) {
    (void)ctx; (void)segment_id;
    return TMK_OK;
}

int tmk_index_invalidate_tombstones(tmk_context_t *ctx) {
    (void)ctx;
    return TMK_OK;
}
