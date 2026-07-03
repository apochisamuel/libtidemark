#ifndef TMK_COMPACT_SWEEP_H
#define TMK_COMPACT_SWEEP_H

#include "../../include/tidemark/types.h"

/*
 * Sweep retired segments from the deferred-free queue.
 * Frees segment data buffers for segments whose compact_refcount
 * has reached zero and whose enqueue epoch is old enough.
 *
 * THIS IS THE FREE SITE for bug #1 (stale_cursor_compact).
 */
int tmk_compact_sweep(tmk_context_t *ctx);

#endif
