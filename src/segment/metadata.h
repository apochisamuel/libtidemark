#ifndef TMK_SEGMENT_METADATA_H
#define TMK_SEGMENT_METADATA_H

#include "../../include/tidemark/types.h"

int tmk_segment_meta_init(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_segment_meta_update(tmk_context_t *ctx, uint16_t segment_id);
tmk_segment_meta_t *tmk_segment_meta_get(tmk_context_t *ctx, uint16_t segment_id);

#endif
