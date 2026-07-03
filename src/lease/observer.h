#ifndef TMK_LEASE_OBSERVER_H
#define TMK_LEASE_OBSERVER_H

#include "../../include/tidemark/types.h"

int tmk_lease_count_active(tmk_context_t *ctx);
int tmk_lease_count_for_session(tmk_context_t *ctx, uint32_t session_id);
int tmk_lease_count_for_segment(tmk_context_t *ctx, uint16_t segment_id);

#endif
