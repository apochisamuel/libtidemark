#ifndef TMK_COMPACT_TOMBSTONE_H
#define TMK_COMPACT_TOMBSTONE_H

#include "../../include/tidemark/types.h"

int tmk_compact_sweep_tombstones(tmk_context_t *ctx);
int tmk_compact_tombstone_count(tmk_context_t *ctx);

#endif
