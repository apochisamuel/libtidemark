#ifndef TMK_API_DISPATCH_H
#define TMK_API_DISPATCH_H

#include "../../include/tidemark/types.h"

/*
 * Master frame dispatcher — routes parsed frames to the appropriate
 * subsystem handler based on frame type.
 */
int tmk_dispatch_frame(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
