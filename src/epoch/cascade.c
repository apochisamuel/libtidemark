/*
 * libtidemark — epoch cascade
 *
 * When the global epoch crosses a generation boundary (every
 * TMK_GENERATION_BOUNDARY advances), the cascade fires a chain
 * of maintenance operations:
 *
 * 1. Increment global_generation
 * 2. Evaluate retention watermarks (may retire segments)
 * 3. Evict expired channels (TTL-based)
 * 4. Enqueue retired segments to deferred-free queue
 * 5. Sweep deferred queue (frees segment data)
 * 6. Sweep tombstoned index nodes (frees index entries)
 * 7. Revoke expired leases
 *
 * This cascade is the ONLY path through which segments, index nodes,
 * channels, and leases are freed.  No frame type directly triggers
 * these operations.
 */
#include "cascade.h"
#include "../retention/enforcer.h"
#include "../retention/evict.h"
#include "../compact/sweep.h"
#include "../compact/defer.h"
#include "../compact/tombstone.h"
#include "../compact/revoke.h"
#include "../../include/tidemark/error.h"

void tmk_epoch_cascade_check(tmk_context_t *ctx) {
    if (!ctx) return;

    if ((ctx->global_epoch % TMK_GENERATION_BOUNDARY) != 0)
        return;

    ctx->global_generation++;

    /* Step 1: Evaluate retention watermarks */
    tmk_retention_evaluate_watermarks(ctx);

    /* Step 1b: Mark index nodes for retired segments as tombstoned */
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (!ctx->index_nodes[i].active) continue;
        tmk_segment_t *seg = NULL;
        for (int j = 0; j < TMK_MAX_SEGMENTS; j++) {
            if (ctx->segments[j].id == ctx->index_nodes[i].segment_id &&
                ctx->segments[j].occupied) {
                seg = &ctx->segments[j];
                break;
            }
        }
        if (seg && (seg->phase == TMK_PHASE_RETIRING ||
                    seg->phase == TMK_PHASE_RETIRED ||
                    seg->tombstone)) {
            if (!ctx->index_nodes[i].tombstone) {
                ctx->index_nodes[i].tombstone = true;
                ctx->tombstone_count++;
            }
        }
    }

    /* Step 2: Evict expired channels */
    tmk_retention_evict_expired_channels(ctx);

    /* Step 3: Enqueue newly retired segments */
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied &&
            ctx->segments[i].phase == TMK_PHASE_RETIRING) {
            tmk_compact_defer_enqueue(ctx, ctx->segments[i].id);
            ctx->segments[i].phase = TMK_PHASE_RETIRED;
        }
    }

    /* Step 4: Sweep deferred queue (frees segment data) */
    tmk_compact_sweep(ctx);

    /* Step 5: Sweep tombstoned index nodes */
    tmk_compact_sweep_tombstones(ctx);

    /* Step 6: Revoke expired leases */
    tmk_compact_revoke_expired(ctx);
}
