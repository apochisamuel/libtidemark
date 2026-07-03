#ifndef TMK_INDEX_NOTIFY_H
#define TMK_INDEX_NOTIFY_H

#include "../../include/tidemark/types.h"

/*
 * Index notification — would propagate segment/partition events
 * to cursors. Currently a stub.
 */
int tmk_index_notify_segment_freed(tmk_context_t *ctx, uint16_t segment_id);
int tmk_index_notify_partition_merged(tmk_context_t *ctx, uint16_t partition_id);

#endif
