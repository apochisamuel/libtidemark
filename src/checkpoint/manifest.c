/*
 * libtidemark — checkpoint manifest utilities
 */
#include "../internal/context.h"
#include "manifest.h"
#include "../../include/tidemark/error.h"

int tmk_checkpoint_manifest_size(tmk_context_t *ctx, uint16_t checkpoint_id) {
    tmk_checkpoint_t *cp = tmk_find_checkpoint(ctx, checkpoint_id);
    if (!cp || !cp->active) return TMK_ERR_NOT_FOUND;
    return (int)cp->segment_count;
}

bool tmk_checkpoint_contains_segment(tmk_context_t *ctx, uint16_t checkpoint_id,
                                     uint16_t segment_id) {
    tmk_checkpoint_t *cp = tmk_find_checkpoint(ctx, checkpoint_id);
    if (!cp || !cp->active) return false;
    for (uint16_t i = 0; i < cp->segment_count; i++) {
        if (cp->segment_ids[i] == segment_id) return true;
    }
    return false;
}

int tmk_checkpoint_invalidate(tmk_context_t *ctx, uint16_t checkpoint_id) {
    tmk_checkpoint_t *cp = tmk_find_checkpoint(ctx, checkpoint_id);
    if (!cp) return TMK_ERR_NOT_FOUND;
    cp->active = false;
    cp->segment_count = 0;
    return TMK_OK;
}
