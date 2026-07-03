#ifndef TMK_MIGRATE_CONTROLLER_H
#define TMK_MIGRATE_CONTROLLER_H

#include "../../include/tidemark/types.h"

int tmk_migrate_begin(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);
int tmk_migrate_transfer(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
