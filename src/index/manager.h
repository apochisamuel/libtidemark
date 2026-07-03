#ifndef TMK_INDEX_MANAGER_H
#define TMK_INDEX_MANAGER_H

#include "../../include/tidemark/types.h"

int tmk_index_register(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_index_remove(tmk_context_t *ctx, uint16_t index_id);

#endif
