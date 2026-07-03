#ifndef TMK_MATERIALIZE_EMIT_H
#define TMK_MATERIALIZE_EMIT_H

#include "../../include/tidemark/types.h"

/*
 * Emit materialization — reads segment data through a cursor's
 * cached data pointer.
 *
 * THIS IS THE CRASH SITE for bug #1 (stale_cursor_compact).
 */
int tmk_materialize_emit(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
