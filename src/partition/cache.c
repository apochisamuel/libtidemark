/*
 * libtidemark — partition epoch cache
 *
 * Caches a pointer to the owning session's epoch state for fast
 * epoch validation during frame dispatch.
 *
 * BUG SURFACE (epoch_session_stale): This cached pointer is NEVER
 * cleared when the session closes.  No session-close observer
 * protocol exists for partitions.
 */
#include "../internal/context.h"
#include "cache.h"
#include "../epoch/session.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_partition_cache_epoch(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t part_id = frame->header.partition_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    if (frame->header.owner_token != part->owner_token)
        return TMK_ERR_BAD_OWNER;

    /* Require epoch state subrecord */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_EPOCH_STATE)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Cache the session's epoch state pointer */
    tmk_epoch_state_t *es = tmk_epoch_get_session(ctx, frame->header.session_id);
    if (!es) return TMK_ERR_NOT_FOUND;

    part->cached_epoch = es;

    return TMK_OK;
}
