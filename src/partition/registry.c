/*
 * libtidemark — partition registry
 *
 * Partitions are the primary organizational unit for time-series data.
 * Each partition owns a set of segments and is bound to channels for
 * ingestion.  Partition metadata includes owner tokens for access
 * control and generation counters for cross-partition coordination.
 */
#include "../internal/context.h"
#include "registry.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_partition_create(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t part_id = frame->header.partition_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    /* Reject duplicate */
    if (tmk_find_partition(ctx, part_id)) return TMK_ERR_ALREADY_EXISTS;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (!ctx->partitions[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_PARTITION_FULL;

    /* Require control subrecord with owner token */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    ctx->partitions[slot].id = part_id;
    ctx->partitions[slot].active = true;
    ctx->partitions[slot].owner_token = frame->header.owner_token;
    ctx->partitions[slot].segment_count = 0;
    ctx->partitions[slot].channel_count = 0;
    ctx->partitions[slot].cached_epoch = NULL;
    ctx->partitions[slot].generation = ctx->global_generation;
    ctx->partitions[slot].create_epoch = ctx->global_epoch;
    ctx->partitions[slot].occupancy = 0;
    ctx->partitions[slot].phase = TMK_PHASE_ACTIVE;
    memset(ctx->partitions[slot].segment_ids, 0,
           sizeof(ctx->partitions[slot].segment_ids));
    memset(ctx->partitions[slot].channel_ids, 0,
           sizeof(ctx->partitions[slot].channel_ids));

    return TMK_OK;
}

int tmk_partition_destroy(tmk_context_t *ctx, uint16_t partition_id) {
    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part) return TMK_ERR_NOT_FOUND;
    part->active = false;
    part->phase = TMK_PHASE_FREED;
    part->cached_epoch = NULL;
    return TMK_OK;
}

tmk_partition_t *tmk_partition_get(tmk_context_t *ctx, uint16_t partition_id) {
    return tmk_find_partition(ctx, partition_id);
}

