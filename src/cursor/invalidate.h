#ifndef TMK_CURSOR_INVALIDATE_H
#define TMK_CURSOR_INVALIDATE_H

#include "../../include/tidemark/types.h"

/*
 * Cursor invalidation — would be the fix for stale_cursor_compact.
 * Currently a stub that does nothing.
 */
int tmk_cursor_invalidate_for_segment(tmk_context_t *ctx, uint16_t segment_id);

#endif
