/*
 * libtidemark — lease observation utilities
 *
 * Provides counting and enumeration of active leases for use by
 * other subsystems (session close validation, segment lifecycle
 * checks, compaction eligibility).
 */
#include "observer.h"
#include "../../include/tidemark/error.h"

int tmk_lease_count_active(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active) count++;
    }
    return count;
}

int tmk_lease_count_for_session(tmk_context_t *ctx, uint32_t session_id) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].session_id == session_id)
            count++;
    }
    return count;
}

int tmk_lease_count_for_segment(tmk_context_t *ctx, uint16_t segment_id) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].segment_id == segment_id)
            count++;
    }
    return count;
}
