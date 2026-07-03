#include "../internal/context.h"
/*
 * libtidemark — session close orchestration
 *
 * Coordinates the multi-step session teardown process:
 * 1. Check for outstanding leases (must be zero)
 * 2. Check for active cursors bound to session
 * 3. Check for pending migrations involving session
 * 4. Prepare epoch state for release
 * 5. Finalize and mark session as retired
 */
#include "close.h"
#include "manager.h"
#include "../../include/tidemark/error.h"

bool tmk_session_can_close(tmk_context_t *ctx, uint32_t session_id) {
    tmk_session_t *sess = tmk_session_get_active(ctx, session_id);
    if (!sess) return false;
    if (sess->lease_count > 0) return false;

    /* Check for active cursors bound to this session */
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active && ctx->cursors[i].session_id == session_id)
            return false;
    }

    /* Check for active migrations */
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active) return false;
    }

    return true;
}

int tmk_session_prepare_close(tmk_context_t *ctx, uint32_t session_id) {
    if (!tmk_session_can_close(ctx, session_id))
        return TMK_ERR_BAD_STATE;

    tmk_session_t *sess = tmk_session_get_active(ctx, session_id);
    if (!sess) return TMK_ERR_NOT_FOUND;

    sess->phase = TMK_PHASE_RETIRING;
    return TMK_OK;
}

int tmk_session_finalize_close(tmk_context_t *ctx, uint32_t session_id) {
    tmk_session_t *sess = NULL;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].id == session_id &&
            ctx->sessions[i].phase == TMK_PHASE_RETIRING) {
            sess = &ctx->sessions[i];
            break;
        }
    }
    if (!sess) return TMK_ERR_NOT_FOUND;

    sess->phase = TMK_PHASE_RETIRED;
    sess->active = false;
    return TMK_OK;
}

/* ═══════════════════════════════════════════════════════════
 * Session close — extended cleanup and graceful shutdown
 * ═══════════════════════════════════════════════════════════ */

int tmk_session_close_graceful(tmk_context_t *ctx, uint32_t session_id) {
    tmk_session_t *sess = tmk_find_session(ctx, session_id);
    if (!sess || !sess->active) return TMK_ERR_NOT_FOUND;

    /* Phase 1: close all cursors */
    int cursor_closed = 0;
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active &&
            ctx->cursors[i].session_id == session_id) {
            ctx->cursors[i].active = false;
            ctx->cursors[i].cached_data = NULL;
            ctx->cursors[i].cached_capacity = 0;
            ctx->cursors[i].bound_lease = NULL;
            ctx->cursors[i].attached_index = NULL;
            cursor_closed++;
        }
    }

    /* Phase 2: release all leases */
    int lease_released = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active &&
            ctx->leases[i].session_id == session_id) {
            /* Decrement segment lease refcount before releasing */
            if (ctx->leases[i].segment_id > 0) {
                tmk_segment_t *seg = tmk_find_segment(ctx,
                    ctx->leases[i].segment_id);
                if (seg && seg->lease_refcount > 0) {
                    seg->lease_refcount--;
                }
            }
            ctx->leases[i].active = false;
            ctx->leases[i].phase = TMK_PHASE_FREED;
            ctx->leases[i].bound_data = NULL;
            ctx->leases[i].bound_metadata = NULL;
            lease_released++;
        }
    }

    /* Phase 3: unbind channels */
    int channel_unbound = 0;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active &&
            ctx->channels[i].session_id == session_id) {
            ctx->channels[i].active = false;
            ctx->channels[i].cached_segment_list = NULL;
            ctx->channels[i].cached_lease = NULL;
            ctx->channels[i].cached_epoch = NULL;
            channel_unbound++;
        }
    }

    /* Phase 4: cancel active migrations for this session */
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active &&
            ctx->migrations[i].owner_token == sess->id) {
            ctx->migrations[i].active = false;
            ctx->migrations[i].phase = TMK_PHASE_FREED;
        }
    }

    /* Phase 5: mark session as closed */
    sess->active = false;
    sess->phase = TMK_PHASE_FREED;
    sess->lease_count = 0;

    /* Update active session tracking */
    if (ctx->active_session_id == session_id) {
        ctx->active_session_id = 0;
    }

    return cursor_closed + lease_released + channel_unbound;
}

int tmk_session_force_close(tmk_context_t *ctx, uint32_t session_id) {
    /* Force close without graceful cleanup — just mark everything dead */
    tmk_session_t *sess = tmk_find_session(ctx, session_id);
    if (!sess) return TMK_ERR_NOT_FOUND;

    sess->active = false;
    sess->phase = TMK_PHASE_FREED;

    /* Mark all related cursors inactive (but DON'T clear cached_data —
     * this is intentional for deferred close scenarios) */
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active &&
            ctx->cursors[i].session_id == session_id) {
            ctx->cursors[i].active = false;
        }
    }

    /* Mark all related leases inactive */
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active &&
            ctx->leases[i].session_id == session_id) {
            ctx->leases[i].active = false;
            ctx->leases[i].phase = TMK_PHASE_FREED;
        }
    }

    return TMK_OK;
}

int tmk_session_health_check(tmk_context_t *ctx, uint32_t session_id) {
    tmk_session_t *sess = tmk_find_session(ctx, session_id);
    if (!sess || !sess->active) return TMK_ERR_NOT_FOUND;

    int health = 0;

    /* Check for dangling cursor pointers */
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (!ctx->cursors[i].active) continue;
        if (ctx->cursors[i].session_id != session_id) continue;

        /* Verify segment still exists */
        tmk_segment_t *seg = tmk_find_segment(ctx, ctx->cursors[i].segment_id);
        if (!seg || !seg->occupied) {
            health |= 0x01; /* dangling cursor */
        }

        /* Verify cached_data consistency */
        if (ctx->cursors[i].cached_data && seg && seg->data) {
            if (ctx->cursors[i].cached_data != seg->data) {
                health |= 0x02; /* stale cached pointer */
            }
        }

        /* Verify capacity consistency */
        if (seg && ctx->cursors[i].cached_capacity != seg->capacity) {
            health |= 0x04; /* capacity mismatch */
        }
    }

    /* Check for dangling lease bindings */
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (!ctx->leases[i].active) continue;
        if (ctx->leases[i].session_id != session_id) continue;

        if (ctx->leases[i].segment_id > 0) {
            tmk_segment_t *seg = tmk_find_segment(ctx,
                ctx->leases[i].segment_id);
            if (!seg || !seg->occupied) {
                health |= 0x08; /* dangling lease */
            }
        }
    }

    /* Check epoch consistency */
    if (sess->epoch > ctx->global_epoch) {
        health |= 0x10; /* session epoch ahead of global */
    }

    return health; /* 0 = healthy, nonzero = issues */
}
