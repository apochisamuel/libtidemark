#ifndef TMK_INTERNAL_CONTEXT_H
#define TMK_INTERNAL_CONTEXT_H

#include "../../include/tidemark/types.h"
#include "../../include/tidemark/error.h"

tmk_context_t *tmk_context_create(void);
void           tmk_context_destroy(tmk_context_t *ctx);
void           tmk_context_reset(tmk_context_t *ctx);

tmk_session_t   *tmk_find_session(tmk_context_t *ctx, uint32_t id);
tmk_channel_t   *tmk_find_channel(tmk_context_t *ctx, uint16_t id);
tmk_partition_t *tmk_find_partition(tmk_context_t *ctx, uint16_t id);
tmk_segment_t   *tmk_find_segment(tmk_context_t *ctx, uint16_t id);
tmk_cursor_t    *tmk_find_cursor(tmk_context_t *ctx, uint16_t id);
tmk_lease_t     *tmk_find_lease(tmk_context_t *ctx, uint16_t id);
tmk_index_node_t *tmk_find_index_node(tmk_context_t *ctx, uint16_t id);
tmk_checkpoint_t *tmk_find_checkpoint(tmk_context_t *ctx, uint16_t id);
tmk_migration_t  *tmk_find_migration(tmk_context_t *ctx, uint16_t id);

#endif
