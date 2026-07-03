#ifndef TMK_PARTITION_REGISTRY_H
#define TMK_PARTITION_REGISTRY_H

#include "../../include/tidemark/types.h"

int tmk_partition_create(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_partition_destroy(tmk_context_t *ctx, uint16_t partition_id);
tmk_partition_t *tmk_partition_get(tmk_context_t *ctx, uint16_t partition_id);
int tmk_partition_add_segment(tmk_context_t *ctx, uint16_t partition_id,
                              uint16_t segment_id);
int tmk_partition_remove_segment(tmk_context_t *ctx, uint16_t partition_id,
                                 uint16_t segment_id);

#endif
