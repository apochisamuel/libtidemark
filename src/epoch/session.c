/*
 * libtidemark — per-session epoch state (heap-allocated)
 *
 * Each session gets a heap-allocated epoch state record.
 * Partitions cache pointers to these records.
 * When a session closes, the epoch state is freed but
 * cached pointers in partitions are NOT invalidated.
 */
#include "session.h"
#include <stdlib.h>
#include "../../include/tidemark/error.h"

int tmk_epoch_init_session(tmk_context_t *ctx, uint32_t session_id,
                           uint32_t initial_epoch) {
    int slot = -1;
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (!ctx->epoch_states[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_SESSION_FULL;

    /* Heap-allocate epoch state */
    tmk_epoch_state_t *es = (tmk_epoch_state_t *)calloc(1, sizeof(tmk_epoch_state_t));
    if (!es) return TMK_ERR_ALLOC;

    es->session_id = session_id;
    es->global_epoch = initial_epoch;
    es->generation = ctx->global_generation;
    es->advance_count = 0;
    es->active = true;

    /* Store in both array (for lookups) and heap pointer (for free) */
    ctx->epoch_states[slot] = *es;
    ctx->epoch_heap[slot] = es;

    return TMK_OK;
}

void tmk_epoch_destroy_session(tmk_context_t *ctx, uint32_t session_id) {
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->epoch_states[i].active &&
            ctx->epoch_states[i].session_id == session_id) {
            ctx->epoch_states[i].active = false;
            /* Free the heap copy */
            if (ctx->epoch_heap[i]) {
                free(ctx->epoch_heap[i]);
                ctx->epoch_heap[i] = NULL;
            }
            return;
        }
    }
}

tmk_epoch_state_t *tmk_epoch_get_session(tmk_context_t *ctx, uint32_t session_id) {
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->epoch_states[i].active &&
            ctx->epoch_states[i].session_id == session_id) {
            /* Return the HEAP pointer so caching it creates a UAF risk */
            return ctx->epoch_heap[i] ? ctx->epoch_heap[i] : &ctx->epoch_states[i];
        }
    }
    return NULL;
}
