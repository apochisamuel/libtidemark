#ifndef TMK_INDEX_INSERT_H
#define TMK_INDEX_INSERT_H

#include "../../include/tidemark/types.h"

int tmk_index_insert(tmk_context_t *ctx, uint16_t index_id,
                     uint32_t key, const uint8_t *value, size_t len);

#endif
