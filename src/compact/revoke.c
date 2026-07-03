/*
 * libtidemark — expired lease revocation (called by cascade)
 *
 * During epoch cascade (epoch → retention → compact), this function
 * revokes leases whose generation is older than the current generation.
 *
 * BUG SURFACE (lease_epoch_cascade): Channel caches that hold
 * pointers to revoked lease contexts are NOT notified.  The channel's
 * cached_lease pointer becomes dangling after the lease is freed here.
 */
#include "revoke.h"
#include "../../include/tidemark/error.h"

int tmk_compact_revoke_expired(tmk_context_t *ctx) {
    int revoked = 0;

    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (!ctx->leases[i].active) continue;

        /* Revoke leases from old generations */
        if (ctx->leases[i].generation + 1 < ctx->global_generation) {
            ctx->leases[i].active = false;
            ctx->leases[i].phase = TMK_PHASE_FREED;

            /* Decrement session lease count */
            for (int j = 0; j < TMK_MAX_SESSIONS; j++) {
                if (ctx->sessions[j].active &&
                    ctx->sessions[j].id == ctx->leases[i].session_id) {
                    if (ctx->sessions[j].lease_count > 0)
                        ctx->sessions[j].lease_count--;
                    break;
                }
            }

            revoked++;
        }
    }

    return revoked;
}
