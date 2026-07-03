/*
 * libtidemark — segment lifecycle transitions
 *
 * Manages the state machine: INIT → ACTIVE → SEALED → RETIRING → RETIRED → FREED.
 * Checks reference counts before allowing transitions to FREED.
 * Note: only checks compact_refcount, NOT lease bindings or cursor caches.
 */
#include "../internal/context.h"
#include "lifecycle.h"
#include "../../include/tidemark/error.h"

bool tmk_segment_can_free(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg) return false;

    /* Only check compaction-side refcount */
    if (seg->compact_refcount > 0) return false;

    /* Phase must be RETIRED or RETIRING */
    if (seg->phase != TMK_PHASE_RETIRED && seg->phase != TMK_PHASE_RETIRING)
        return false;

    return true;
}

int tmk_segment_transition(tmk_context_t *ctx, uint16_t segment_id,
                           tmk_phase_t new_phase) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    /* Validate transition is forward */
    if ((int)new_phase <= (int)seg->phase)
        return TMK_ERR_NOT_ACTIVE;

    /* Check references before freeing */
    if (new_phase == TMK_PHASE_FREED) {
        if (!tmk_segment_can_free(ctx, segment_id))
            return TMK_ERR_BAD_STATE;
    }

    seg->phase = new_phase;
    return TMK_OK;
}

int tmk_segment_check_refs(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg) return TMK_ERR_NOT_FOUND;

    int refs = 0;

    /* Count compact refcount */
    refs += (int)seg->compact_refcount;

    /* Count lease bindings (separate refcount) */
    refs += (int)seg->lease_refcount;

    return refs;
}
