#ifndef TMK_CHANNEL_IWRITE_H
#define TMK_CHANNEL_IWRITE_H

#include "../../include/tidemark/types.h"

/*
 * Channel indexed write — data ingestion with simultaneous
 * index updates.  Routes data through the channel binding to
 * the target segment, then updates the associated index node.
 *
 * The indexed write path is more expensive than plain DATA_INGEST
 * but maintains real-time index consistency.
 *
 * BUG SURFACE (cursor_gen_realloc): If cursor table compaction
 * occurs during an indexed write, index→cursor pointers may be
 * stale, causing the next cursor read through the index to access
 * freed memory.
 */

int tmk_channel_index_write(tmk_context_t *ctx, const tmk_parsed_frame_t *frame);

#endif
