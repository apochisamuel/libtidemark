/*
 * libtidemark — materialize emit
 *
 * Reads segment data through the cursor's cached_data pointer.
 * The cursor was opened with a direct pointer to the segment's
 * data buffer.  If the segment has been freed by compaction,
 * this read accesses freed memory.
 *
 * THIS IS THE READ SITE / CRASH SITE for stale_cursor_compact.
 */
#include "../internal/context.h"
#include "emit.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_materialize_emit(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CURSOR_POS)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t cursor_id = frame->header.route_id;
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    /* FIX: cursor may have been invalidated by segment-lifecycle notification */
    if (!cur->cached_data) return TMK_ERR_NOT_ACTIVE;

    /*
     * READ THROUGH CACHED POINTER.
     *
     * BUG: if the segment was freed by tmk_compact_sweep(), this
     * pointer is dangling.  ASAN will report heap-use-after-free here.
     *
     * The missing abstraction: compact/ should have notified index/
     * which should have propagated to cursor/ to invalidate this
     * cached pointer before freeing the segment.
     */
    size_t read_offset = cur->position;
    if (read_offset < cur->cached_capacity) {
        volatile uint8_t data = cur->cached_data[read_offset];
        (void)data;
        cur->position++;
    }

    return TMK_OK;
}
