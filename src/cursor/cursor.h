#ifndef TMK_CURSOR_CURSOR_H
#define TMK_CURSOR_CURSOR_H

#include "../../include/tidemark/types.h"

int tmk_cursor_open(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_cursor_close(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
