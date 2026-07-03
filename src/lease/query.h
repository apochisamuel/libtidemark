#ifndef TMK_LEASE_QUERY_H
#define TMK_LEASE_QUERY_H

#include "../../include/tidemark/types.h"

tmk_lease_t *tmk_lease_find_by_token(tmk_context_t *ctx, uint32_t token);
tmk_lease_t *tmk_lease_find_by_segment(tmk_context_t *ctx, uint16_t segment_id);
bool tmk_lease_is_valid(tmk_context_t *ctx, uint16_t lease_id);

#endif
