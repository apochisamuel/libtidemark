#ifndef TMK_CHECKPOINT_RESTORE_H
#define TMK_CHECKPOINT_RESTORE_H

#include "../../include/tidemark/types.h"

int tmk_checkpoint_restore(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_checkpoint_restore_segment(tmk_context_t *ctx, uint16_t checkpoint_id,
                                   uint16_t segment_id);

#endif
