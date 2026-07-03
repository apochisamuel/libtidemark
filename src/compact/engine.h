#ifndef TMK_COMPACT_ENGINE_H
#define TMK_COMPACT_ENGINE_H

#include "../../include/tidemark/types.h"

int tmk_compact_schedule(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_compact_trigger(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
