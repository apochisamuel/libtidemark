/*
 * libtidemark — lease-segment binding
 *
 * Binds a lease to a specific segment's data buffer.  The lease
 * stores a direct pointer to the segment's data for fast read
 * access.  This binding is used by the deferred_segment_free bug.
 */
#include "../internal/context.h"
#include "bind.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_lease_bind(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_LEASE_TOKEN)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint32_t token = frame->subrecords[0].lease_token;
    uint16_t seg_id = frame->header.route_id;

    /* Find the lease */
    tmk_lease_t *lease = NULL;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].token == token) {
            lease = &ctx->leases[i];
            break;
        }
    }
    if (!lease) return TMK_ERR_NOT_FOUND;

    /* Find the segment */
    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    /* Bind: cache data pointer and increment refcount */
    lease->segment_id = seg_id;
    lease->bound_data = seg->data;
    lease->bound_capacity = seg->capacity;
    lease->bound_metadata = seg->meta;  /* cache metadata pointer */
    seg->lease_refcount++;

    return TMK_OK;
}

/*
 * PATCH: Clear bound_data when a segment is about to be freed.
 * Called by compact/sweep after confirming no checkpoint holds a snapshot.
 */
int tmk_lease_clear_binding(tmk_context_t *ctx, uint16_t segment_id) {
    int cleared = 0;
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg) return 0;

    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].bound_data == seg->data) {
            ctx->leases[i].bound_data = NULL;
            ctx->leases[i].bound_capacity = 0;
            cleared++;
        }
    }
    return cleared;
}
