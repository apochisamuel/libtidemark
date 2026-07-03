/*
 * libtidemark — migration lease revocation
 *
 * Revokes leases bound to segments in a specific partition.
 * Called during migration commit for the source partition.
 *
 * BUG SURFACE (lease_transfer_dangle): Cursors bound to revoked
 * leases are NOT notified.  The cursor's bound_lease pointer
 * becomes dangling.
 */
#include "../internal/context.h"
#include "revoke.h"
#include "../../include/tidemark/error.h"

int tmk_migrate_revoke_leases(tmk_context_t *ctx, uint16_t partition_id) {
    int revoked = 0;

    /* Find all segments in this partition */
    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part) return TMK_ERR_NOT_FOUND;

    /* Revoke leases bound to these segments */
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (!ctx->leases[i].active) continue;

        /* Check if lease is bound to a segment in this partition */
        tmk_segment_t *seg = tmk_find_segment(ctx, ctx->leases[i].segment_id);
        if (seg && seg->partition_id == partition_id) {
            ctx->leases[i].active = false;
            ctx->leases[i].phase = TMK_PHASE_FREED;
            /* bound_data is NOT cleared — this is intentional for
             * deferred_segment_free bug surface */
            revoked++;

            /* Decrement session lease count */
            for (int j = 0; j < TMK_MAX_SESSIONS; j++) {
                if (ctx->sessions[j].active &&
                    ctx->sessions[j].id == ctx->leases[i].session_id) {
                    if (ctx->sessions[j].lease_count > 0)
                        ctx->sessions[j].lease_count--;
                    break;
                }
            }
        }
    }

    return revoked;
}
