/*
 * libtidemark — segment allocation and lifecycle
 *
 * Segments are the fundamental storage unit.  Each segment owns a
 * contiguous data buffer for time-series records.  Segment creation
 * allocates both the segment slot and its data buffer; retirement
 * marks the segment for compaction but does not immediately free
 * the data buffer.
 *
 * Cursors, leases, and index entries may cache pointers to segment
 * data buffers.  When a segment is freed (by compaction sweep or
 * migration retirement), these cached pointers must be invalidated —
 * but no cross-subsystem notification protocol exists for this.
 */
#include "../internal/context.h"
#include "alloc.h"
#include "../partition/registry.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"
#include <stdlib.h>
#include <string.h>

int tmk_segment_create(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t seg_id = frame->header.route_id;
    uint16_t part_id = frame->header.partition_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    /* Reject duplicate */
    if (tmk_find_segment(ctx, seg_id)) return TMK_ERR_ALREADY_EXISTS;

    /* Validate partition exists */
    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    /* Validate owner token */
    if (frame->header.owner_token != part->owner_token)
        return TMK_ERR_BAD_OWNER;

    /* Require capacity info subrecord */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CAPACITY_INFO)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Extract capacity from subrecord payload */
    uint32_t capacity = 4096; /* default */
    if (frame->subrecords[0].payload && frame->subrecords[0].length >= 4) {
        capacity = (uint32_t)frame->subrecords[0].payload[0] |
                   ((uint32_t)frame->subrecords[0].payload[1] << 8) |
                   ((uint32_t)frame->subrecords[0].payload[2] << 16) |
                   ((uint32_t)frame->subrecords[0].payload[3] << 24);
    }
    if (capacity > TMK_MAX_SEGMENT_CAP) capacity = TMK_MAX_SEGMENT_CAP;
    if (capacity < 64) capacity = 64;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (!ctx->segments[i].occupied) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_SEGMENT_FULL;

    /* Allocate data buffer */
    uint8_t *data = (uint8_t *)calloc(1, capacity);
    if (!data) return TMK_ERR_ALLOC;

    ctx->segments[slot].id = seg_id;
    ctx->segments[slot].partition_id = part_id;
    ctx->segments[slot].phase = TMK_PHASE_ACTIVE;
    ctx->segments[slot].data = data;
    ctx->segments[slot].capacity = capacity;
    ctx->segments[slot].used = 0;
    ctx->segments[slot].generation = ctx->global_generation;
    ctx->segments[slot].create_epoch = ctx->global_epoch;
    ctx->segments[slot].owner_token = frame->header.owner_token;
    ctx->segments[slot].compact_refcount = 0;
    ctx->segments[slot].lease_refcount = 0;
    ctx->segments[slot].tombstone = false;
    ctx->segments[slot].occupied = true;
    ctx->segments[slot].meta = (tmk_segment_meta_t *)calloc(1, sizeof(tmk_segment_meta_t));
    if (ctx->segments[slot].meta) {
        ctx->segments[slot].meta->checksum = 0;
        ctx->segments[slot].meta->record_count = 0;
    }

    /* Register segment in partition */
    tmk_partition_add_segment(ctx, part_id, seg_id);

    return TMK_OK;
}

int tmk_segment_free(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg) return TMK_ERR_NOT_FOUND;

    /*
     * Free the data buffer.
     *
     * BUG SURFACE: cursors, leases, and index entries may still hold
     * cached pointers to this buffer.  No notification is sent to
     * those subsystems.
     */
    if (seg->data) {
        free(seg->data);
        seg->data = NULL;
    }
    if (seg->meta) {
        free(seg->meta);
        seg->meta = NULL;
    }

    /* Remove from partition */
    tmk_partition_remove_segment(ctx, seg->partition_id, segment_id);

    seg->occupied = false;
    seg->phase = TMK_PHASE_FREED;
    seg->capacity = 0;
    seg->used = 0;

    return TMK_OK;
}

int tmk_segment_retire(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t seg_id = frame->header.route_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;
    if (seg->phase != TMK_PHASE_ACTIVE && seg->phase != TMK_PHASE_SEALED)
        return TMK_ERR_NOT_ACTIVE;

    /* Require control subrecord */
    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    seg->phase = TMK_PHASE_RETIRING;
    seg->tombstone = true;
    ctx->tombstone_count++;

    return TMK_OK;
}
