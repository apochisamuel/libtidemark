/*
 * libtidemark — cursor table compaction
 *
 * When cursor churn (open/close cycles) exceeds a threshold,
 * the cursor table can be compacted by reassigning slot indices.
 *
 * BUG SURFACE (cursor_gen_realloc): Index entries that cached
 * pointers to cursor position state are NOT updated after
 * compaction.
 */
#include "compact.h"
#include "../../include/tidemark/error.h"

int tmk_cursor_compact_table(tmk_context_t *ctx) {
    if (ctx->cursor_churn_count < TMK_CURSOR_COMPACT_THRESHOLD)
        return TMK_OK;

    /* Compact: move all active cursors to front of table */
    int write_idx = 0;
    for (int read_idx = 0; read_idx < TMK_MAX_CURSORS; read_idx++) {
        if (ctx->cursors[read_idx].active) {
            if (write_idx != read_idx) {
                ctx->cursors[write_idx] = ctx->cursors[read_idx];
                ctx->cursors[read_idx].active = false;
                ctx->cursors[read_idx].cached_data = NULL;
                /*
                 * BUG: index entries with cursor_ref pointing to
                 * slot read_idx are NOT updated to point to write_idx.
                 */
            }
            write_idx++;
        }
    }

    ctx->cursor_churn_count = 0;
    return TMK_OK;
}
