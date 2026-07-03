/*
 * libtidemark — channel ingestion API
 *
 * Handles multi-channel data ingestion, channel registration,
 * partition/segment creation, and epoch management.
 * Own dispatcher — no shared dispatch with other APIs.
 */
#include "../internal/context.h"
#include "../session/manager.h"
#include "../session/close.h"
#include "../channel/register.h"
#include "../channel/bind.h"
#include "../channel/cache.h"
#include "../partition/observer.h"
#include "../retention/enforcer.h"
#include "../channel/validate.h"
#include "../channel/iwrite.h"
#include "../partition/registry.h"
#include "../partition/bind.h"
#include "../partition/cache.h"
#include "../segment/alloc.h"
#include "../segment/store.h"
#include "../epoch/tracker.h"
#include "../epoch/cascade.h"
#include "../lease/acquire.h"
#include "../wire/parser.h"
#include "../../include/tidemark/tidemark.h"
#include "../../include/tidemark/error.h"

static int ch_dispatch(tmk_context_t *ctx,
                       const tmk_parsed_frame_t *frame, void *user) {
    (void)user;
    int rc;

    switch (frame->header.frame_type) {
    case TMK_FRAME_SESSION_OPEN:
        return tmk_session_open(ctx, frame);

    case TMK_FRAME_SESSION_CLOSE:
        return tmk_session_close(ctx, frame);

    case TMK_FRAME_CHANNEL_REGISTER:
        return tmk_channel_register(ctx, frame);

    case TMK_FRAME_CHANNEL_BIND:
        return tmk_channel_bind_partition(ctx, frame);

    case TMK_FRAME_CHANNEL_CACHE_LEASE:
        return tmk_channel_cache_lease(ctx, frame);

    case TMK_FRAME_PARTITION_CREATE:
        return tmk_partition_create(ctx, frame);

    case TMK_FRAME_PARTITION_BIND:
        return tmk_partition_bind_channel(ctx, frame);

    case TMK_FRAME_PARTITION_CACHE_EPOCH:
        return tmk_partition_cache_epoch(ctx, frame);

    case TMK_FRAME_SEGMENT_CREATE:
        return tmk_segment_create(ctx, frame);

    case TMK_FRAME_DATA_INGEST:
        /* Validate epoch through partition cache */
        if (frame->header.partition_id > 0) {
            tmk_channel_validate_epoch(ctx, frame->header.partition_id, frame);
        }
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

    case TMK_FRAME_LEASE_ACQUIRE:
        return tmk_lease_acquire(ctx, frame);

    case TMK_FRAME_EPOCH_ADVANCE:
        rc = tmk_epoch_advance(ctx, frame);
        if (rc == TMK_OK) tmk_epoch_cascade_check(ctx);
        return rc;

    case TMK_FRAME_RETENTION_TRIGGER:
        return tmk_retention_trigger(ctx, frame);

    case TMK_FRAME_RETENTION_CONFIG:
        return tmk_retention_configure(ctx, frame);

    case TMK_FRAME_PARTITION_VALIDATE:
        return tmk_partition_validate_channel_desc(ctx, frame->header.partition_id);

    default:
        return TMK_ERR_BAD_FRAME_TYPE;
    }
}

int tmk_channel_ingest(tmk_context_t *ctx, const uint8_t *buf, size_t len) {
    if (!ctx || !buf || len == 0) return TMK_ERR_SHORT_BUF;
    return tmk_parse_message(ctx, buf, len, ch_dispatch, NULL);
}
