#ifndef TMK_EPOCH_TRACKER_H
#define TMK_EPOCH_TRACKER_H

#include "../../include/tidemark/types.h"

int tmk_epoch_advance(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
uint32_t tmk_epoch_current(tmk_context_t *ctx);
bool tmk_epoch_crossed_boundary(tmk_context_t *ctx);
int tmk_epoch_validate_frame(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
