#ifndef TMK_SEGMENT_LIFECYCLE_H
#define TMK_SEGMENT_LIFECYCLE_H

#include "../../include/tidemark/types.h"

bool tmk_segment_can_free(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_transition(tmk_context_t *ctx, uint16_t segment_id,
                           tmk_phase_t new_phase);
int tmk_segment_check_refs(tmk_context_t *ctx, uint16_t segment_id);

#endif
