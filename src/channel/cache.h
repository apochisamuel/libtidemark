#ifndef TMK_CHANNEL_CACHE_H
#define TMK_CHANNEL_CACHE_H

#include "../../include/tidemark/types.h"

int tmk_channel_cache_lease(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_channel_invalidate_cache(tmk_context_t *ctx, uint16_t channel_id);

#endif
