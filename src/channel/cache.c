/*
 * libtidemark — channel cache management
 *
 * Channels cache lease contexts for fast dispatch validation.
 * When a frame arrives, the channel checks its cached lease for
 * validity rather than performing a full lease lookup.
 *
 * BUG SURFACE (lease_epoch_cascade): If an epoch cascade triggers
 * lease revocation (through retention → compact → lease-revoke),
 * the channel's cached lease pointer becomes dangling because the
 * cascade chain does not notify channel caches.
 */
#include "../internal/context.h"
#include "cache.h"
#include "../lease/acquire.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_channel_cache_lease(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t ch_id = frame->header.channel_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    tmk_channel_t *ch = tmk_find_channel(ctx, ch_id);
    if (!ch || !ch->active) return TMK_ERR_NOT_FOUND;

    /* Subrecord must contain lease token */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_LEASE_TOKEN)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint32_t lease_token = frame->subrecords[0].lease_token;

    /* Find the lease by token */
    tmk_lease_t *lease = NULL;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].token == lease_token) {
            lease = &ctx->leases[i];
            break;
        }
    }
    if (!lease) return TMK_ERR_NOT_FOUND;

    /* Cache the lease pointer for fast dispatch */
    ch->cached_lease = lease;

    return TMK_OK;
}

int tmk_channel_invalidate_cache(tmk_context_t *ctx, uint16_t channel_id) {
    tmk_channel_t *ch = tmk_find_channel(ctx, channel_id);
    if (!ch) return TMK_ERR_NOT_FOUND;
    ch->cached_lease = NULL;
    ch->cached_segment_list = NULL;
    ch->cached_epoch = NULL;
    return TMK_OK;
}
