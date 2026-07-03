#ifndef TMK_RETENTION_EVICT_H
#define TMK_RETENTION_EVICT_H

#include "../../include/tidemark/types.h"

int tmk_retention_evict_channel(tmk_context_t *ctx, uint16_t channel_id);
int tmk_retention_evict_expired_channels(tmk_context_t *ctx);

#endif
