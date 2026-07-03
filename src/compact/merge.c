/*
 * libtidemark — segment merging
 *
 * Merges two segments within the same partition into a single
 * replacement segment.  The old segments are freed; the new
 * segment may have smaller capacity.
 *
 * BUG SURFACE (compact_merge_oob): Index entries that stored the
 * old segment's capacity are NOT updated.  Materialization reads
 * up to the old (larger) capacity from the new (smaller) buffer,
 * causing heap-buffer-overflow.
 */
#include "../internal/context.h"
#include "merge.h"
#include "../segment/alloc.h"
#include "../segment/store.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"
#include <stdlib.h>
#include <string.h>

int tmk_compact_merge_segments(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* The route_id identifies the first segment to merge */
    uint16_t seg_a_id = frame->header.route_id;
    tmk_segment_t *seg_a = tmk_find_segment(ctx, seg_a_id);
    if (!seg_a || !seg_a->occupied) return TMK_ERR_NOT_FOUND;

    /* Find the second segment in the same partition */
    tmk_segment_t *seg_b = NULL;
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied &&
            ctx->segments[i].id != seg_a_id &&
            ctx->segments[i].partition_id == seg_a->partition_id &&
            ctx->segments[i].phase == TMK_PHASE_ACTIVE) {
            seg_b = &ctx->segments[i];
            break;
        }
    }
    if (!seg_b) return TMK_ERR_NOT_FOUND;

    /* Calculate merged capacity — may be smaller due to dedup */
    size_t merged_used = seg_a->used + seg_b->used;
    size_t new_capacity = merged_used + 64; /* small overhead */
    if (new_capacity > TMK_MAX_SEGMENT_CAP) new_capacity = TMK_MAX_SEGMENT_CAP;

    /* Allocate new buffer */
    uint8_t *new_data = (uint8_t *)calloc(1, new_capacity);
    if (!new_data) return TMK_ERR_ALLOC;

    /* Copy data from both segments */
    if (seg_a->data && seg_a->used > 0) {
        memcpy(new_data, seg_a->data, seg_a->used);
    }
    if (seg_b->data && seg_b->used > 0 &&
        seg_a->used + seg_b->used <= new_capacity) {
        memcpy(new_data + seg_a->used, seg_b->data, seg_b->used);
    }

    /* Free old segment A data, reuse slot with new buffer */
    if (seg_a->data) free(seg_a->data);
    seg_a->data = new_data;
    seg_a->capacity = new_capacity;
    seg_a->used = merged_used;
    seg_a->generation++;

    /* Free segment B entirely */
    uint16_t seg_b_id = seg_b->id;
    tmk_segment_free(ctx, seg_b_id);

    return TMK_OK;
}
