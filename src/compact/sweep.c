/*
 * libtidemark — compaction sweep
 *
 * Drains the deferred-free queue by freeing segments whose compact
 * refcount has reached zero and whose deferred age exceeds the
 * sweep threshold.
 */
#include "../internal/context.h"
#include "sweep.h"
#include "defer.h"
#include "../segment/alloc.h"
#include "../segment/lifecycle.h"
#include "../../include/tidemark/error.h"

int tmk_compact_sweep(tmk_context_t *ctx) {
    int freed_count = 0;

    for (int i = 0; i < TMK_MAX_DEFERRED; i++) {
        if (!ctx->deferred[i].occupied) continue;

        uint16_t seg_id = ctx->deferred[i].segment_id;
        tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
        if (!seg) {
            ctx->deferred[i].occupied = false;
            ctx->deferred_count--;
            continue;
        }

        /* Check if segment can be freed (compact_refcount == 0) */
        if (seg->compact_refcount > 0) continue;

        /* Check epoch age */
        uint32_t age = ctx->global_epoch - ctx->deferred[i].enqueue_epoch;
        if (age < TMK_COMPACT_SWEEP_THRESHOLD) continue;

        /*
         * FREE THE SEGMENT DATA BUFFER.
         *
         * BUG: No cross-subsystem coordination exists here.
         * Cursor subsystem may still have cached_data pointing
         * to this buffer.  Checkpoint subsystem may still have
         * data_snapshots referencing it.  Lease subsystem may
         * still have bound_data pointing here.
         *
         * None of these are consulted before freeing.
         */
        tmk_segment_free(ctx, seg_id);

        ctx->deferred[i].occupied = false;
        ctx->deferred_count--;
        freed_count++;
    }

    return freed_count;
}
