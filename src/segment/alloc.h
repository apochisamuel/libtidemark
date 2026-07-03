#ifndef TMK_SEGMENT_ALLOC_H
#define TMK_SEGMENT_ALLOC_H

#include "../../include/tidemark/types.h"

int tmk_segment_create(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_segment_free(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_retire(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
