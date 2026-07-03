#ifndef TMK_COMPACT_DEFER_H
#define TMK_COMPACT_DEFER_H

#include "../../include/tidemark/types.h"

int tmk_compact_defer_enqueue(tmk_context_t *ctx, uint16_t segment_id);
int tmk_compact_defer_drain(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_compact_defer_count(tmk_context_t *ctx);

#endif
