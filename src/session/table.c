#include "../internal/context.h"
/*
 * libtidemark — session table management
 *
 * Maintains occupancy statistics and provides generation-aware lookups
 * for the session table.  The GC routine cleans up sessions in RETIRED
 * phase that have fully drained their resources.
 */
#include "table.h"
#include "../../include/tidemark/error.h"

int tmk_session_table_count_active(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active) count++;
    }
    return count;
}

int tmk_session_table_find_by_epoch(tmk_context_t *ctx, uint32_t epoch,
                                    tmk_session_t **out, int max) {
    int found = 0;
    for (int i = 0; i < TMK_MAX_SESSIONS && found < max; i++) {
        if (ctx->sessions[i].active && ctx->sessions[i].epoch <= epoch) {
            out[found++] = &ctx->sessions[i];
        }
    }
    return found;
}

int tmk_session_table_gc(tmk_context_t *ctx) {
    int cleaned = 0;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].phase == TMK_PHASE_RETIRED &&
            ctx->sessions[i].lease_count == 0) {
            ctx->sessions[i].active = false;
            ctx->sessions[i].phase = TMK_PHASE_FREED;
            cleaned++;
        }
    }
    return cleaned;
}

/* ═══════════════════════════════════════════════════════════
 * Session table management — extended operations
 * ═══════════════════════════════════════════════════════════ */

int tmk_session_table_compact(tmk_context_t *ctx) {
    /* Compact session table: move active sessions to front */
    int write_idx = 0;
    int moved = 0;
    for (int read_idx = 0; read_idx < TMK_MAX_SESSIONS; read_idx++) {
        if (ctx->sessions[read_idx].active) {
            if (write_idx != read_idx) {
                ctx->sessions[write_idx] = ctx->sessions[read_idx];
                ctx->sessions[read_idx].active = false;
                moved++;
            }
            write_idx++;
        }
    }
    return moved;
}

int tmk_session_count_active(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active) count++;
    }
    return count;
}

uint32_t tmk_session_oldest_epoch(tmk_context_t *ctx) {
    uint32_t oldest = UINT32_MAX;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active && ctx->sessions[i].epoch < oldest) {
            oldest = ctx->sessions[i].epoch;
        }
    }
    return (oldest == UINT32_MAX) ? 0 : oldest;
}

uint32_t tmk_session_newest_epoch(tmk_context_t *ctx) {
    uint32_t newest = 0;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active && ctx->sessions[i].epoch > newest) {
            newest = ctx->sessions[i].epoch;
        }
    }
    return newest;
}

int tmk_session_gc_stale(tmk_context_t *ctx, uint32_t max_age_epochs) {
    int gc_count = 0;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (!ctx->sessions[i].active) continue;
        uint32_t age = ctx->global_epoch - ctx->sessions[i].epoch;
        if (age > max_age_epochs) {
            ctx->sessions[i].active = false;
            ctx->sessions[i].phase = TMK_PHASE_FREED;
            gc_count++;
        }
    }
    return gc_count;
}

int tmk_session_enumerate_leases(tmk_context_t *ctx, uint32_t session_id,
                                  uint16_t *out_ids, uint16_t max_count,
                                  uint16_t *out_count) {
    if (!out_ids || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    for (int i = 0; i < TMK_MAX_LEASES && *out_count < max_count; i++) {
        if (ctx->leases[i].active && ctx->leases[i].session_id == session_id) {
            out_ids[*out_count] = ctx->leases[i].id;
            (*out_count)++;
        }
    }
    return TMK_OK;
}

int tmk_session_enumerate_cursors(tmk_context_t *ctx, uint32_t session_id,
                                   uint16_t *out_ids, uint16_t max_count,
                                   uint16_t *out_count) {
    if (!out_ids || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    for (int i = 0; i < TMK_MAX_CURSORS && *out_count < max_count; i++) {
        if (ctx->cursors[i].active && ctx->cursors[i].session_id == session_id) {
            out_ids[*out_count] = ctx->cursors[i].id;
            (*out_count)++;
        }
    }
    return TMK_OK;
}

int tmk_session_revoke_all_leases(tmk_context_t *ctx, uint32_t session_id) {
    int revoked = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].session_id == session_id) {
            ctx->leases[i].active = false;
            ctx->leases[i].phase = TMK_PHASE_FREED;

            /* Decrement segment refcount */
            if (ctx->leases[i].segment_id > 0) {
                tmk_segment_t *seg = tmk_find_segment(ctx,
                    ctx->leases[i].segment_id);
                if (seg && seg->lease_refcount > 0)
                    seg->lease_refcount--;
            }
            revoked++;
        }
    }
    return revoked;
}

int tmk_session_close_all_cursors(tmk_context_t *ctx, uint32_t session_id) {
    int closed = 0;
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active && ctx->cursors[i].session_id == session_id) {
            ctx->cursors[i].active = false;
            ctx->cursors[i].cached_data = NULL;
            ctx->cursors[i].cached_capacity = 0;
            ctx->cursors[i].bound_lease = NULL;
            ctx->cursors[i].attached_index = NULL;
            closed++;
        }
    }
    return closed;
}

int tmk_session_validate_integrity(tmk_context_t *ctx, uint32_t session_id) {
    tmk_session_t *sess = tmk_find_session(ctx, session_id);
    if (!sess || !sess->active) return TMK_ERR_NOT_FOUND;

    int issues = 0;

    /* Check lease count consistency */
    int actual_leases = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].session_id == session_id)
            actual_leases++;
    }
    if ((uint32_t)actual_leases != sess->lease_count) issues++;

    /* Check for orphaned cursors (pointing to freed segments) */
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (!ctx->cursors[i].active) continue;
        if (ctx->cursors[i].session_id != session_id) continue;

        tmk_segment_t *seg = tmk_find_segment(ctx, ctx->cursors[i].segment_id);
        if (!seg || !seg->occupied) {
            issues++; /* cursor points to freed segment */
        }
    }

    return issues;
}
