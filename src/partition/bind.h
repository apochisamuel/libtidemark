#ifndef TMK_PARTITION_BIND_H
#define TMK_PARTITION_BIND_H

#include "../../include/tidemark/types.h"

int tmk_partition_bind_channel(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_partition_unbind_channel(tmk_context_t *ctx, uint16_t partition_id,
                                 uint16_t channel_id);

#endif
