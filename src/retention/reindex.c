/*
 * libtidemark — segment re-indexing
 *
 * Triggered by retention when watermark thresholds are crossed.
 * Re-indexes a segment's metadata by freeing the old metadata
 * struct and allocating a new one.
 *
 * BUG SURFACE (segment_lease_reindex): Leases that hold a pointer
 * to the old metadata struct via bound_metadata are NOT notified.
 * Cursor reads through the lease's stale metadata pointer cause
 * use-after-free.
 */
#include "../internal/context.h"
#include "reindex.h"
#include "../segment/metadata.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"
#include <stdlib.h>
#include <string.h>

int tmk_retention_reindex_segment(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    uint16_t seg_id = frame->header.route_id;
    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_METADATA)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Free old metadata and allocate new */
    if (seg->meta) {
        free(seg->meta);
    }
    seg->meta = (tmk_segment_meta_t *)calloc(1, sizeof(tmk_segment_meta_t));
    if (!seg->meta) return TMK_ERR_ALLOC;

    /* Re-compute metadata */
    seg->meta->checksum = 0;
    if (seg->data && seg->used > 0) {
        /* Compute CRC over segment data */
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < seg->used; i++) {
            crc ^= seg->data[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0x82F63B78u & -(crc & 1));
            }
        }
        seg->meta->checksum = ~crc;
    }
    seg->meta->min_ts = frame->subrecords[0].local_epoch;
    seg->meta->max_ts = frame->subrecords[0].generation;
    seg->meta->record_count = (uint16_t)(seg->used / 8);
    seg->meta->index_depth = 1;

    return TMK_OK;
}
