/*
 * libtidemark — session processing API
 *
 * Handles session lifecycle, epoch management, leases, retention,
 * and checkpoint operations.  This API has its own frame dispatcher;
 * it does NOT share dispatch logic with other API entry points.
 *
 * Compaction is never directly dispatched — it fires only as an
 * internal side-effect of epoch cascade within tmk_epoch_cascade_check.
 */
#include "../internal/context.h"
#include "../session/manager.h"
#include "../session/close.h"
#include "../segment/alloc.h"
#include "../segment/store.h"
#include "../partition/registry.h"
#include "../lease/acquire.h"
#include "../lease/bind.h"
#include "../epoch/tracker.h"
#include "../epoch/cascade.h"
#include "../retention/enforcer.h"
#include "../retention/reindex.h"
#include "../checkpoint/engine.h"
#include "../checkpoint/restore.h"
#include "../wire/parser.h"
#include "../../include/tidemark/tidemark.h"
#include "../../include/tidemark/error.h"

/*
 * Session-API dispatcher.  Handles only session-relevant frame types.
 * Does NOT handle: COMPACT_*, CURSOR_*, MATERIALIZE_*, INDEX_*,
 * MIGRATE_*, CHANNEL_BIND, CHANNEL_REGISTER.
 *
 * Compaction side-effects are triggered internally by epoch cascade,
 * not by any frame type visible in this switch.
 */
static int ss_dispatch(tmk_context_t *ctx,
                       const tmk_parsed_frame_t *frame, void *user) {
    (void)user;
    int rc;

    switch (frame->header.frame_type) {
    case TMK_FRAME_SESSION_OPEN:
        return tmk_session_open(ctx, frame);

    case TMK_FRAME_SESSION_CLOSE:
        return tmk_session_close(ctx, frame);

    case TMK_FRAME_PARTITION_CREATE:
        return tmk_partition_create(ctx, frame);

    case TMK_FRAME_SEGMENT_CREATE:
        return tmk_segment_create(ctx, frame);

    case TMK_FRAME_DATA_INGEST:
        if (frame->parsed_sub_count >= 1 &&
            frame->subrecords[0].kind == TMK_SUB_DATA) {
            uint16_t seg_id = frame->header.route_id;
            if (frame->subrecords[0].payload && frame->subrecords[0].length > 0) {
                tmk_segment_write(ctx, seg_id,
                                  frame->subrecords[0].payload,
                                  frame->subrecords[0].length);
            }
        }
        return TMK_OK;

    case TMK_FRAME_LEASE_ACQUIRE:
        return tmk_lease_acquire(ctx, frame);

    case TMK_FRAME_LEASE_RELEASE:
        return tmk_lease_release(ctx, frame);

    case TMK_FRAME_LEASE_BIND:
        return tmk_lease_bind(ctx, frame);

    case TMK_FRAME_EPOCH_ADVANCE:
        rc = tmk_epoch_advance(ctx, frame);
        if (rc == TMK_OK) {
            /* Side-effect: epoch cascade may trigger retention
             * and compaction internally.  No frame type exposes
             * this — it is purely a consequence of crossing
             * generation boundaries and retention watermarks. */
            tmk_epoch_cascade_check(ctx);
        }
        return rc;

    case TMK_FRAME_RETENTION_TRIGGER:
        return tmk_retention_trigger(ctx, frame);

    case TMK_FRAME_RETENTION_REINDEX:
        return tmk_retention_reindex_segment(ctx, frame);

    case TMK_FRAME_CHECKPOINT_CREATE:
        return tmk_checkpoint_create(ctx, frame);

    case TMK_FRAME_CHECKPOINT_RESTORE:
        return tmk_checkpoint_restore(ctx, frame);

    default:
        return TMK_ERR_BAD_FRAME_TYPE;
    }
}

int tmk_session_process(tmk_context_t *ctx, const uint8_t *buf, size_t len) {
    if (!ctx || !buf || len == 0) return TMK_ERR_SHORT_BUF;
    return tmk_parse_message(ctx, buf, len, ss_dispatch, NULL);
}
