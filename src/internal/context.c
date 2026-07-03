/*
 * libtidemark — context creation, destruction, and lookup
 */
#include "context.h"
#include <stdlib.h>
#include <string.h>

tmk_context_t *tmk_context_create(void) {
    tmk_context_t *ctx = (tmk_context_t *)calloc(1, sizeof(tmk_context_t));
    if (!ctx) return NULL;
    ctx->global_epoch = 1;
    ctx->global_generation = 1;
    ctx->global_sequence = 0;
    ctx->retention.watermark_epoch = 0;
    ctx->retention.ttl_epochs = TMK_CHANNEL_TTL_EPOCHS;
    ctx->retention.cascade_enabled = false;
    ctx->retention.configured = false;
    return ctx;
}

static void destroy_segments(tmk_context_t *ctx) {
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied && ctx->segments[i].data) {
            free(ctx->segments[i].data);
            ctx->segments[i].data = NULL;
        }
        if (ctx->segments[i].meta) {
            free(ctx->segments[i].meta);
            ctx->segments[i].meta = NULL;
        }
    }
}

static void destroy_cursors(tmk_context_t *ctx) {
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        ctx->cursors[i].active = false;
        ctx->cursors[i].cached_data = NULL;
    }
}

static void destroy_leases(tmk_context_t *ctx) {
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        ctx->leases[i].active = false;
        ctx->leases[i].bound_data = NULL;
        ctx->leases[i].bound_metadata = NULL;
    }
}

static void destroy_channels(tmk_context_t *ctx) {
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].channel_data) {
            free(ctx->channels[i].channel_data);
            ctx->channels[i].channel_data = NULL;
        }
    }
}

static void destroy_epoch_states(tmk_context_t *ctx) {
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->epoch_heap[i]) {
            free(ctx->epoch_heap[i]);
            ctx->epoch_heap[i] = NULL;
        }
        ctx->epoch_states[i].active = false;
    }
}

void tmk_context_destroy(tmk_context_t *ctx) {
    if (!ctx) return;
    destroy_cursors(ctx);
    destroy_leases(ctx);
    destroy_channels(ctx);
    destroy_epoch_states(ctx);
    destroy_segments(ctx);
    free(ctx);
}

void tmk_context_reset(tmk_context_t *ctx) {
    if (!ctx) return;
    destroy_cursors(ctx);
    destroy_leases(ctx);
    destroy_channels(ctx);
    destroy_epoch_states(ctx);
    destroy_segments(ctx);
    memset(ctx, 0, sizeof(tmk_context_t));
    ctx->global_epoch = 1;
    ctx->global_generation = 1;
}

tmk_session_t *tmk_find_session(tmk_context_t *ctx, uint32_t id) {
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active && ctx->sessions[i].id == id)
            return &ctx->sessions[i];
    }
    return NULL;
}

tmk_channel_t *tmk_find_channel(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active && ctx->channels[i].id == id)
            return &ctx->channels[i];
    }
    return NULL;
}

tmk_partition_t *tmk_find_partition(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (ctx->partitions[i].active && ctx->partitions[i].id == id)
            return &ctx->partitions[i];
    }
    return NULL;
}

tmk_segment_t *tmk_find_segment(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied && ctx->segments[i].id == id)
            return &ctx->segments[i];
    }
    return NULL;
}

tmk_cursor_t *tmk_find_cursor(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active && ctx->cursors[i].id == id)
            return &ctx->cursors[i];
    }
    return NULL;
}

tmk_lease_t *tmk_find_lease(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].id == id)
            return &ctx->leases[i];
    }
    return NULL;
}

tmk_index_node_t *tmk_find_index_node(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active && ctx->index_nodes[i].id == id)
            return &ctx->index_nodes[i];
    }
    return NULL;
}

tmk_checkpoint_t *tmk_find_checkpoint(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_CHECKPOINTS; i++) {
        if (ctx->checkpoints[i].active && ctx->checkpoints[i].id == id)
            return &ctx->checkpoints[i];
    }
    return NULL;
}

tmk_migration_t *tmk_find_migration(tmk_context_t *ctx, uint16_t id) {
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active && ctx->migrations[i].id == id)
            return &ctx->migrations[i];
    }
    return NULL;
}
