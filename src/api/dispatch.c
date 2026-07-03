/*
 * libtidemark — master frame dispatcher
 *
 * Routes each parsed frame to the correct subsystem handler.
 * This is the central coordination point for all incoming operations.
 */
#include "dispatch.h"
#include "../session/manager.h"
#include "../channel/register.h"
#include "../channel/bind.h"
#include "../channel/cache.h"
#include "../channel/router.h"
#include "../segment/store.h"
#include "../channel/iwrite.h"
#include "../partition/registry.h"
#include "../partition/bind.h"
#include "../partition/cache.h"
#include "../segment/alloc.h"
#include "../segment/metadata.h"
#include "../epoch/tracker.h"
#include "../epoch/cascade.h"
#include "../cursor/cursor.h"
#include "../cursor/bind.h"
#include "../cursor/attach.h"
#include "../lease/acquire.h"
#include "../lease/bind.h"
#include "../lease/metadata.h"
#include "../index/manager.h"
#include "../index/bind.h"
#include "../index/cross.h"
#include "../index/capacity.h"
#include "../retention/enforcer.h"
#include "../retention/reindex.h"
#include "../compact/engine.h"
#include "../compact/merge.h"
#include "../compact/partition.h"
#include "../compact/defer.h"
#include "../checkpoint/engine.h"
#include "../checkpoint/restore.h"
#include "../migrate/controller.h"
#include "../migrate/commit.h"
#include "../migrate/enumerate.h"
#include "../materialize/engine.h"
#include "../../include/tidemark/error.h"

int tmk_dispatch_frame(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int rc;

    switch (frame->header.frame_type) {
    /* Session */
    case TMK_FRAME_SESSION_OPEN:
        return tmk_session_open(ctx, frame);
    case TMK_FRAME_SESSION_CLOSE:
        return tmk_session_close(ctx, frame);

    /* Channel */
    case TMK_FRAME_CHANNEL_REGISTER:
        return tmk_channel_register(ctx, frame);
    case TMK_FRAME_CHANNEL_BIND:
        return tmk_channel_bind_partition(ctx, frame);
    case TMK_FRAME_CHANNEL_CACHE_LEASE:
        return tmk_channel_cache_lease(ctx, frame);

    /* Partition */
    case TMK_FRAME_PARTITION_CREATE:
        return tmk_partition_create(ctx, frame);
    case TMK_FRAME_PARTITION_BIND:
        return tmk_partition_bind_channel(ctx, frame);
    case TMK_FRAME_PARTITION_CACHE_EPOCH:
        return tmk_partition_cache_epoch(ctx, frame);

    /* Segment */
    case TMK_FRAME_SEGMENT_CREATE:
        return tmk_segment_create(ctx, frame);
    case TMK_FRAME_SEGMENT_RETIRE:
        return tmk_segment_retire(ctx, frame);
    case TMK_FRAME_SEGMENT_META_INIT:
        return tmk_segment_meta_init(ctx, frame);

    /* Data */
    case TMK_FRAME_DATA_INGEST:
        /* Route through channel to segment */
        rc = tmk_channel_route_frame(ctx, frame);
        if (rc < 0) return rc;
        /* Write data subrecord to segment */
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

    case TMK_FRAME_DATA_INGEST_IXWR:
        return tmk_channel_index_write(ctx, frame);

    /* Index */
    case TMK_FRAME_INDEX_REGISTER:
        return tmk_index_register(ctx, frame);
    case TMK_FRAME_INDEX_BUILD_CROSS:
        return tmk_index_build_cross_partition(ctx, frame);
    case TMK_FRAME_INDEX_BIND_CURSOR:
        return tmk_index_bind_cursor(ctx, frame);
    case TMK_FRAME_INDEX_REG_CAPACITY:
        return tmk_index_register_capacity(ctx, frame);

    /* Cursor */
    case TMK_FRAME_CURSOR_OPEN:
        return tmk_cursor_open(ctx, frame);
    case TMK_FRAME_CURSOR_CLOSE:
        return tmk_cursor_close(ctx, frame);
    case TMK_FRAME_CURSOR_ATTACH_IDX:
        return tmk_cursor_attach_index(ctx, frame);
    case TMK_FRAME_CURSOR_BIND_LEASE:
        return tmk_cursor_bind_lease(ctx, frame);

    /* Lease */
    case TMK_FRAME_LEASE_ACQUIRE:
        return tmk_lease_acquire(ctx, frame);
    case TMK_FRAME_LEASE_RELEASE:
        return tmk_lease_release(ctx, frame);
    case TMK_FRAME_LEASE_BIND:
        return tmk_lease_bind(ctx, frame);
    case TMK_FRAME_LEASE_BIND_META:
        return tmk_lease_bind_metadata(ctx, frame);

    /* Epoch */
    case TMK_FRAME_EPOCH_ADVANCE:
        rc = tmk_epoch_advance(ctx, frame);
        if (rc == TMK_OK) {
            tmk_epoch_cascade_check(ctx);
        }
        return rc;

    /* Retention */
    case TMK_FRAME_RETENTION_TRIGGER:
        return tmk_retention_trigger(ctx, frame);
    case TMK_FRAME_RETENTION_CONFIG:
        return tmk_retention_configure(ctx, frame);
    case TMK_FRAME_RETENTION_REINDEX:
        return tmk_retention_reindex_segment(ctx, frame);

    /* Compact */
    case TMK_FRAME_COMPACT_SCHEDULE:
        return tmk_compact_schedule(ctx, frame);
    case TMK_FRAME_COMPACT_TRIGGER:
        return tmk_compact_trigger(ctx, frame);
    case TMK_FRAME_COMPACT_MERGE:
        return tmk_compact_merge_segments(ctx, frame);
    case TMK_FRAME_COMPACT_MERGE_PART:
        return tmk_compact_merge_partitions(ctx, frame);
    case TMK_FRAME_COMPACT_DEFER_DRAIN:
        return tmk_compact_defer_drain(ctx, frame);

    /* Checkpoint */
    case TMK_FRAME_CHECKPOINT_CREATE:
        return tmk_checkpoint_create(ctx, frame);
    case TMK_FRAME_CHECKPOINT_RESTORE:
        return tmk_checkpoint_restore(ctx, frame);

    /* Migrate */
    case TMK_FRAME_MIGRATE_BEGIN:
        return tmk_migrate_begin(ctx, frame);
    case TMK_FRAME_MIGRATE_TRANSFER:
        return tmk_migrate_transfer(ctx, frame);
    case TMK_FRAME_MIGRATE_COMMIT:
        return tmk_migrate_commit(ctx, frame);
    case TMK_FRAME_MIGRATE_ENUMERATE:
        return tmk_migrate_enumerate_channels(ctx, frame);

    /* Materialize */
    case TMK_FRAME_MATERIALIZE_EMIT:
    case TMK_FRAME_MATERIALIZE_SCAN:
    case TMK_FRAME_MATERIALIZE_RANGE:
    case TMK_FRAME_MATERIALIZE_RESOLVE:
    case TMK_FRAME_MATERIALIZE_READ:
    case TMK_FRAME_MATERIALIZE_ISCAN:
        return tmk_materialize_dispatch(ctx, frame);

    default:
        return TMK_ERR_BAD_FRAME_TYPE;
    }
}
