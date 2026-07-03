#ifndef TMK_CHANNEL_ROUTER_H
#define TMK_CHANNEL_ROUTER_H

#include "../../include/tidemark/types.h"

int tmk_channel_route_frame(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
tmk_partition_t *tmk_channel_resolve_partition(tmk_context_t *ctx, uint16_t channel_id);

#endif
