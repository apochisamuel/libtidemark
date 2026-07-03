#ifndef TMK_RETENTION_ENFORCER_H
#define TMK_RETENTION_ENFORCER_H

#include "../../include/tidemark/types.h"

int tmk_retention_trigger(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_retention_configure(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_retention_evaluate_watermarks(tmk_context_t *ctx);

#endif
