#ifndef TMK_SESSION_CLOSE_H
#define TMK_SESSION_CLOSE_H

#include "../../include/tidemark/types.h"

/*
 * Session close operations — graceful shutdown, forced close,
 * and session health checking.
 */

int tmk_session_close(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_session_close_graceful(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_force_close(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_health_check(tmk_context_t *ctx, uint32_t session_id);

#endif
