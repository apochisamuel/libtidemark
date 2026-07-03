#ifndef TMK_SEGMENT_STORE_H
#define TMK_SEGMENT_STORE_H

#include "../../include/tidemark/types.h"

int tmk_segment_write(tmk_context_t *ctx, uint16_t segment_id,
                      const uint8_t *data, size_t len);
int tmk_segment_read(tmk_context_t *ctx, uint16_t segment_id,
                     uint8_t *out, size_t offset, size_t len);
int tmk_segment_seal(tmk_context_t *ctx, uint16_t segment_id);

#endif
