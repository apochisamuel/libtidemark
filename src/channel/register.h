#ifndef TMK_CHANNEL_REGISTER_H
#define TMK_CHANNEL_REGISTER_H

#include "../../include/tidemark/types.h"

int tmk_channel_register(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_channel_unregister(tmk_context_t *ctx, uint16_t channel_id);

#endif
