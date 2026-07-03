/*
 * libtidemark — stream/query execution API
 *
 * Handles cursor operations, materialization, index management.
 * Own dispatcher — no shared dispatch with other APIs.
 */
#include "../internal/context.h"
#include "../session/manager.h"
#include "../cursor/cursor.h"
#include "../cursor/bind.h"
#include "../cursor/attach.h"
#include "../cursor/check.h"
#include "../cursor/seek.h"
#include "../index/manager.h"
#include "../index/bind.h"
#include "../index/cross.h"
#include "../index/capacity.h"
#include "../segment/alloc.h"
#include "../segment/store.h"
#include "../partition/registry.h"
#include "../channel/register.h"
#include "../channel/bind.h"
#include "../lease/acquire.h"
#include "../lease/bind.h"
#include "../materialize/engine.h"
#include "../materialize/aggregate.h"
#include "../compact/merge.h"
#include "../index/register.h"
#include "../epoch/tracker.h"
#include "../epoch/cascade.h"
#include "../retention/enforcer.h"
#include "../retention/reindex.h"
#include "../wire/parser.h"
#include "../../include/tidemark/tidemark.h"
#include "../../include/tidemark/error.h"

static int st_dispatch(tmk_context_t *ctx,
                       const tmk_parsed_frame_t *frame, void *user) {
    (void)user;
    int rc;

    switch (frame->header.frame_type) {
    case TMK_FRAME_SESSION_OPEN:
        return tmk_session_open(ctx, frame);

    case TMK_FRAME_CHANNEL_REGISTER:
        return tmk_channel_register(ctx, frame);

    case TMK_FRAME_CHANNEL_BIND:
        return tmk_channel_bind_partition(ctx, frame);

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

    case TMK_FRAME_INDEX_REGISTER: {
        uint16_t idx_id = frame->header.route_id;
        uint16_t seg_id = frame->header.partition_id;
        tmk_index_node_t *idx = tmk_find_index_node(ctx, idx_id);
        if (!idx) {
            for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
                if (!ctx->index_nodes[i].active) {
                    ctx->index_nodes[i].id = idx_id;
                    ctx->index_nodes[i].active = true;
                    break;
                }
            }
        }
        return tmk_index_register_segment(ctx, idx_id, seg_id);
    }

    case TMK_FRAME_INDEX_BUILD_CROSS:
        return tmk_index_build_cross_partition(ctx, frame);

    case TMK_FRAME_INDEX_BIND_CURSOR:
        return tmk_index_bind_cursor(ctx, frame);

    case TMK_FRAME_INDEX_REG_CAPACITY:
        return tmk_index_register_capacity(ctx, frame);

    case TMK_FRAME_CURSOR_OPEN:
        return tmk_cursor_open(ctx, frame);

    case TMK_FRAME_CURSOR_CLOSE:
        return tmk_cursor_close(ctx, frame);

    case TMK_FRAME_CURSOR_ATTACH_IDX:
        return tmk_cursor_attach_index(ctx, frame);

    case TMK_FRAME_CURSOR_READ_BATCH: {
        /* Read batch from cursor — crashes if cached_data is dangling */
        uint16_t cid = frame->header.route_id;
        tmk_cursor_t *cur = tmk_find_cursor(ctx, cid);
        if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;
        uint8_t tmp[256];
        size_t bread = 0;
        return tmk_cursor_read_batch(ctx, cid, tmp, sizeof(tmp), &bread);
    }

    case TMK_FRAME_CURSOR_SEEK: {
        /* Seek cursor position */
        uint16_t cid = frame->header.route_id;
        tmk_cursor_t *cur = tmk_find_cursor(ctx, cid);
        if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;
        size_t remaining = 0;
        return tmk_cursor_read_batch(ctx, cid, (uint8_t[256]){0}, 256, &remaining);
    }

    case TMK_FRAME_CURSOR_CHECK:
        return tmk_cursor_check_lease(ctx, frame->header.route_id) == TMK_OK ? TMK_OK : TMK_ERR_NOT_ACTIVE;
    case TMK_FRAME_CURSOR_BIND_LEASE:
        return tmk_cursor_bind_lease(ctx, frame);

    case TMK_FRAME_LEASE_ACQUIRE:
        return tmk_lease_acquire(ctx, frame);

    case TMK_FRAME_LEASE_BIND:
        return tmk_lease_bind(ctx, frame);

    case TMK_FRAME_LEASE_RELEASE:
        return tmk_lease_release(ctx, frame);

    case TMK_FRAME_EPOCH_ADVANCE:
        rc = tmk_epoch_advance(ctx, frame);
        if (rc == TMK_OK) tmk_epoch_cascade_check(ctx);
        return rc;

    case TMK_FRAME_RETENTION_TRIGGER:
        return tmk_retention_trigger(ctx, frame);

    case TMK_FRAME_RETENTION_REINDEX:
        return tmk_retention_reindex_segment(ctx, frame);

    case TMK_FRAME_MATERIALIZE_EMIT:
    case TMK_FRAME_MATERIALIZE_SCAN:
    case TMK_FRAME_MATERIALIZE_RANGE:
    case TMK_FRAME_MATERIALIZE_RESOLVE:
    case TMK_FRAME_MATERIALIZE_READ:
    case TMK_FRAME_COMPACT_MERGE:
        return tmk_compact_merge_segments(ctx, frame);

    case TMK_FRAME_MATERIALIZE_AGG_IDX:
        return tmk_materialize_aggregate_indexed(ctx, frame);

    case TMK_FRAME_MATERIALIZE_ISCAN:
        return tmk_materialize_dispatch(ctx, frame);

    default:
        return TMK_ERR_BAD_FRAME_TYPE;
    }
}

int tmk_stream_execute(tmk_context_t *ctx, const uint8_t *buf, size_t len) {
    if (!ctx || !buf || len == 0) return TMK_ERR_SHORT_BUF;
    return tmk_parse_message(ctx, buf, len, st_dispatch, NULL);
}
