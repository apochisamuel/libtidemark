#ifndef TMK_CHANNEL_DISPATCH_H
#define TMK_CHANNEL_DISPATCH_H

#include "../../include/tidemark/types.h"

/*
 * Channel dispatch — frame-level routing through channel bindings.
 * Dispatches incoming data frames to the correct partition and
 * segment based on channel configuration.
 */

int tmk_channel_dispatch_lease(tmk_context_t *ctx, uint16_t channel_id,
                               const tmk_parsed_frame_t *frame);
int tmk_channel_count_active(tmk_context_t *ctx);
int tmk_channel_count_for_session(tmk_context_t *ctx, uint32_t session_id);
int tmk_channel_count_bound(tmk_context_t *ctx);
int tmk_channel_gc_unbound(tmk_context_t *ctx);
int tmk_channel_enumerate_for_partition(tmk_context_t *ctx,
                                         uint16_t partition_id,
                                         uint16_t *out_ids,
                                         uint16_t max_count,
                                         uint16_t *out_count);
int tmk_channel_invalidate_caches(tmk_context_t *ctx, uint16_t partition_id);

#endif
