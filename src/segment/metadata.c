/*
 * libtidemark — segment metadata management
 *
 * Each segment can have associated metadata (checksums, timestamp
 * ranges, record counts) used for indexing and query planning.
 * Metadata is allocated separately from segment data and can be
 * re-indexed independently.
 *
 * BUG SURFACE (segment_lease_reindex): When retention triggers
 * re-indexing, the old metadata struct is freed and replaced.
 * Leases that cached a pointer to the old metadata are NOT notified.
 */
#include "../internal/context.h"
#include "metadata.h"
#include "../wire/crc.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"
#include <stdlib.h>
#include <string.h>

int tmk_segment_meta_init(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    uint16_t seg_id = frame->header.route_id;

    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_METADATA)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Allocate metadata if not present */
    if (!seg->meta) {
        seg->meta = (tmk_segment_meta_t *)calloc(1, sizeof(tmk_segment_meta_t));
        if (!seg->meta) return TMK_ERR_ALLOC;
    }

    /* Populate from subrecord */
    seg->meta->min_ts = frame->subrecords[0].local_epoch;
    seg->meta->max_ts = frame->subrecords[0].generation;
    seg->meta->record_count = 0;
    seg->meta->index_depth = 0;
    seg->meta->checksum = tmk_crc32c(seg->data, seg->used);

    return TMK_OK;
}

int tmk_segment_meta_update(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied || !seg->meta) return TMK_ERR_NOT_FOUND;

    seg->meta->checksum = tmk_crc32c(seg->data, seg->used);
    seg->meta->record_count++;
    return TMK_OK;
}

tmk_segment_meta_t *tmk_segment_meta_get(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) return NULL;
    return seg->meta;
}
