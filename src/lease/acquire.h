#ifndef TMK_LEASE_ACQUIRE_H
#define TMK_LEASE_ACQUIRE_H

#include "../../include/tidemark/types.h"

int tmk_lease_acquire(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_lease_release(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
