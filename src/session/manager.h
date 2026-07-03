#ifndef TMK_SESSION_MANAGER_H
#define TMK_SESSION_MANAGER_H

#include "../../include/tidemark/types.h"

int tmk_session_open(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_session_close(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
tmk_session_t *tmk_session_get_active(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_validate(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
