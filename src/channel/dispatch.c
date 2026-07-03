/*
 * libtidemark — channel lease dispatch
 *
 * Dispatches incoming frames through the channel's cached lease
 * for validation.  The cached lease is checked for active status
 * and generation compatibility before allowing the frame to proceed
 * to the data path.
 *
 * BUG SURFACE (lease_epoch_cascade): If the cached lease was freed
 * by an epoch cascade, this function reads through the dangling
 * pointer to check lease fields, causing use-after-free.
 */
#include "../internal/context.h"
#include "dispatch.h"
#include "validate.h"
#include "../../include/tidemark/error.h"

int tmk_channel_dispatch_lease(tmk_context_t *ctx, uint16_t channel_id,
                               const tmk_parsed_frame_t *frame) {
    tmk_channel_t *ch = tmk_find_channel(ctx, channel_id);
    if (!ch || !ch->active) return TMK_ERR_NOT_FOUND;

    /* If no cached lease, fall back to full lookup */
    if (!ch->cached_lease) return TMK_OK;

    /*
     * Validate via cached lease pointer.
     * BUG: if lease was freed by epoch cascade, this reads freed memory.
     */
    if (!ch->cached_lease->active) return TMK_ERR_NOT_ACTIVE;
    if (ch->cached_lease->generation != ctx->global_generation)
        return TMK_ERR_BAD_STATE;

    return TMK_OK;
}

/* ═══════════════════════════════════════════════════════════
 * Channel dispatch — extended operations
 * ═══════════════════════════════════════════════════════════ */

int tmk_channel_count_active(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active) count++;
    }
    return count;
}

int tmk_channel_count_for_session(tmk_context_t *ctx, uint32_t session_id) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active &&
            ctx->channels[i].session_id == session_id)
            count++;
    }
    return count;
}

int tmk_channel_count_bound(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active &&
            ctx->channels[i].bound_partition_id > 0)
            count++;
    }
    return count;
}

int tmk_channel_gc_unbound(tmk_context_t *ctx) {
    int gc = 0;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active &&
            ctx->channels[i].bound_partition_id == 0) {
            uint32_t age = ctx->global_epoch - ctx->channels[i].create_epoch;
            if (age > ctx->channels[i].ttl_epochs) {
                ctx->channels[i].active = false;
                gc++;
            }
        }
    }
    return gc;
}

int tmk_channel_enumerate_for_partition(tmk_context_t *ctx,
                                         uint16_t partition_id,
                                         uint16_t *out_ids,
                                         uint16_t max_count,
                                         uint16_t *out_count) {
    if (!out_ids || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    for (int i = 0; i < TMK_MAX_CHANNELS && *out_count < max_count; i++) {
        if (ctx->channels[i].active &&
            ctx->channels[i].bound_partition_id == partition_id) {
            out_ids[*out_count] = ctx->channels[i].id;
            (*out_count)++;
        }
    }
    return TMK_OK;
}

int tmk_channel_invalidate_caches(tmk_context_t *ctx, uint16_t partition_id) {
    int invalidated = 0;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active &&
            ctx->channels[i].bound_partition_id == partition_id) {
            ctx->channels[i].cached_segment_list = NULL;
            ctx->channels[i].cached_lease = NULL;
            ctx->channels[i].cached_epoch = NULL;
            invalidated++;
        }
    }
    return invalidated;
}

/*
 * Validate channel descriptor data during dispatch.
 * BUG SURFACE: If the channel was evicted by retention but the
 * partition still holds a cached channel binding, this reads
 * freed channel_data.
 */
int tmk_channel_validate_descriptor(tmk_context_t *ctx, uint16_t channel_id) {
    tmk_channel_t *ch = tmk_find_channel(ctx, channel_id);
    if (!ch) return -1;
    if (ch->channel_data && ch->channel_data_len > 0) {
        /* Read descriptor data for validation */
        volatile uint8_t check = ch->channel_data[0];
        (void)check;
        return 0;
    }
    return -1;
}
