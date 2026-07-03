/*
 * libtidemark — lease metadata binding
 *
 * Binds a lease to a segment's metadata struct.  Used for metadata
 * validation during cursor reads.
 *
 * BUG SURFACE (segment_lease_reindex): When retention triggers
 * re-indexing, the old metadata struct is freed.  The lease's
 * bound_metadata pointer becomes dangling.
 */
#include "../internal/context.h"
#include "metadata.h"
#include "../segment/metadata.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_lease_bind_metadata(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_LEASE_TOKEN)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint32_t token = frame->subrecords[0].lease_token;
    uint16_t seg_id = frame->header.route_id;

    tmk_lease_t *lease = NULL;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active && ctx->leases[i].token == token) {
            lease = &ctx->leases[i];
            break;
        }
    }
    if (!lease) return TMK_ERR_NOT_FOUND;

    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied || !seg->meta) return TMK_ERR_NOT_FOUND;

    /* Cache metadata pointer */
    lease->bound_metadata = seg->meta;

    return TMK_OK;
}
