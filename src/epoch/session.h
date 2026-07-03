#ifndef TMK_EPOCH_SESSION_H
#define TMK_EPOCH_SESSION_H

#include "../../include/tidemark/types.h"

/*
 * Per-session epoch state management.
 * Each session has its own epoch tracking record for generation-aware
 * operations.  Partition subsystems may cache pointers to these records
 * for fast epoch validation during frame dispatch.
 */
int tmk_epoch_init_session(tmk_context_t *ctx, uint32_t session_id,
                           uint32_t initial_epoch);
void tmk_epoch_destroy_session(tmk_context_t *ctx, uint32_t session_id);
tmk_epoch_state_t *tmk_epoch_get_session(tmk_context_t *ctx, uint32_t session_id);

#endif
