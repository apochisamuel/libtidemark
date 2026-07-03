/*
 * libtidemark — cursor open/close
 *
 * Cursors provide iteration state for reading segment data.  When
 * a cursor is opened, it caches a DIRECT POINTER to the segment's
 * data buffer for zero-copy reads.
 *
 * THIS IS THE ALIAS SITE for bug #1 (stale_cursor_compact).
 * The cached_data pointer is set here and never invalidated when
 * the segment is freed by compaction sweep.
 */
#include "../internal/context.h"
#include "cursor.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_cursor_open(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CURSOR_POS)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t cursor_id = frame->header.route_id;
    uint16_t seg_id = frame->header.channel_id; /* reuse for segment target */

    /* Reject duplicate */
    if (tmk_find_cursor(ctx, cursor_id)) return TMK_ERR_ALREADY_EXISTS;

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (!ctx->cursors[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_CURSOR_FULL;

    /* Find target segment */
    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    ctx->cursors[slot].id = cursor_id;
    ctx->cursors[slot].active = true;
    ctx->cursors[slot].session_id = frame->header.session_id;
    ctx->cursors[slot].segment_id = seg_id;

    /*
     * ALIAS: cache a direct pointer to the segment's data buffer.
     *
     * THIS IS THE BUG — this pointer is never invalidated when the
     * segment is freed by tmk_compact_sweep().  No cursor-invalidation
     * protocol exists between compact/, index/, and cursor/.
     */
    ctx->cursors[slot].cached_data = seg->data;
    ctx->cursors[slot].cached_capacity = seg->capacity;

    ctx->cursors[slot].position = 0;
    ctx->cursors[slot].generation = ctx->global_generation;
    ctx->cursors[slot].create_epoch = ctx->global_epoch;
    ctx->cursors[slot].bound_lease = NULL;
    ctx->cursors[slot].attached_index = NULL;
    ctx->cursors[slot].partition_id = seg->partition_id;

    return TMK_OK;
}

int tmk_cursor_close(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    uint16_t cursor_id = frame->header.route_id;
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur) return TMK_ERR_NOT_FOUND;

    cur->active = false;
    cur->cached_data = NULL;
    cur->cached_capacity = 0;
    cur->bound_lease = NULL;
    cur->attached_index = NULL;

    ctx->cursor_churn_count++;

    return TMK_OK;
}
