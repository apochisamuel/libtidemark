#ifndef TMK_INDEX_INVALIDATE_H
#define TMK_INDEX_INVALIDATE_H

#include "../../include/tidemark/types.h"

int tmk_index_invalidate_for_segment(tmk_context_t *ctx, uint16_t segment_id);
int tmk_index_invalidate_tombstones(tmk_context_t *ctx);

#endif
