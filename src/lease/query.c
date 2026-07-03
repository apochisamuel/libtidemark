/*
 * libtidemark — lease query operations
 *
 * Lookup utilities for finding leases by token, segment, or ID.
 * Used by cursor, materialize, and checkpoint subsystems.
 */
#include "../internal/context.h"
#include "query.h"
#include "../../include/tidemark/error.h"

tmk_lease_t *tmk_lease_find_by_token(tmk_context_t *ctx, uint32_t token) {
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].token == token)
            return &ctx->leases[i];
    }
    return NULL;
}

tmk_lease_t *tmk_lease_find_by_segment(tmk_context_t *ctx, uint16_t segment_id) {
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].segment_id == segment_id)
            return &ctx->leases[i];
    }
    return NULL;
}

bool tmk_lease_is_valid(tmk_context_t *ctx, uint16_t lease_id) {
    tmk_lease_t *l = tmk_find_lease(ctx, lease_id);
    if (!l || !l->active) return false;
    if (l->phase != TMK_PHASE_ACTIVE) return false;
    return true;
}
