/*
 * libtidemark — migration processing API
 *
 * Handles cross-partition migration, checkpointing.
 * Own dispatcher — no shared dispatch with other APIs.
 */
#include "../internal/context.h"
#include "../session/manager.h"
#include "../partition/registry.h"
#include "../segment/alloc.h"
#include "../segment/store.h"
#include "../migrate/controller.h"
#include "../migrate/commit.h"
#include "../migrate/enumerate.h"
#include "../checkpoint/engine.h"
#include "../cursor/cursor.h"
#include "../lease/acquire.h"
#include "../lease/bind.h"
#include "../checkpoint/restore.h"
#include "../epoch/tracker.h"
#include "../epoch/cascade.h"
#include "../materialize/engine.h"
#include "../wire/parser.h"
#include "../../include/tidemark/tidemark.h"
#include "../../include/tidemark/error.h"

static int mg_dispatch(tmk_context_t *ctx,
                       const tmk_parsed_frame_t *frame, void *user) {
    (void)user;
    int rc;

    switch (frame->header.frame_type) {
    case TMK_FRAME_SESSION_OPEN:
        return tmk_session_open(ctx, frame);

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

    case TMK_FRAME_EPOCH_ADVANCE:
        rc = tmk_epoch_advance(ctx, frame);
        if (rc == TMK_OK) tmk_epoch_cascade_check(ctx);
        return rc;

    case TMK_FRAME_MIGRATE_BEGIN:
        return tmk_migrate_begin(ctx, frame);

    case TMK_FRAME_MIGRATE_TRANSFER:
        return tmk_migrate_transfer(ctx, frame);

    case TMK_FRAME_MIGRATE_COMMIT:
        return tmk_migrate_commit(ctx, frame);

    case TMK_FRAME_MIGRATE_ENUMERATE:
        return tmk_migrate_enumerate_channels(ctx, frame);

    case TMK_FRAME_CHECKPOINT_CREATE:
        return tmk_checkpoint_create(ctx, frame);

    case TMK_FRAME_CHECKPOINT_RESTORE:
        return tmk_checkpoint_restore(ctx, frame);

    case TMK_FRAME_MATERIALIZE_SCAN:
    case TMK_FRAME_MATERIALIZE_RESOLVE:
        return tmk_materialize_dispatch(ctx, frame);

    case TMK_FRAME_CURSOR_OPEN:
        return tmk_cursor_open(ctx, frame);

    case TMK_FRAME_LEASE_ACQUIRE:
        return tmk_lease_acquire(ctx, frame);

    case TMK_FRAME_LEASE_BIND:
        return tmk_lease_bind(ctx, frame);

    case TMK_FRAME_LEASE_RELEASE:
        return tmk_lease_release(ctx, frame);

    case TMK_FRAME_MATERIALIZE_EMIT:
        return tmk_materialize_dispatch(ctx, frame);

    default:
        return TMK_ERR_BAD_FRAME_TYPE;
    }
}

int tmk_migrate_process(tmk_context_t *ctx, const uint8_t *buf, size_t len) {
    if (!ctx || !buf || len == 0) return TMK_ERR_SHORT_BUF;
    return tmk_parse_message(ctx, buf, len, mg_dispatch, NULL);
}
