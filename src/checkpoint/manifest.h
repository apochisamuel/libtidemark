#ifndef TMK_CHECKPOINT_MANIFEST_H
#define TMK_CHECKPOINT_MANIFEST_H

#include "../../include/tidemark/types.h"

int tmk_checkpoint_manifest_size(tmk_context_t *ctx, uint16_t checkpoint_id);
bool tmk_checkpoint_contains_segment(tmk_context_t *ctx, uint16_t checkpoint_id,
                                     uint16_t segment_id);
int tmk_checkpoint_invalidate(tmk_context_t *ctx, uint16_t checkpoint_id);

#endif
