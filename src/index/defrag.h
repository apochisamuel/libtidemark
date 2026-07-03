#ifndef TMK_INDEX_DEFRAG_H
#define TMK_INDEX_DEFRAG_H

#include "../../include/tidemark/types.h"

/*
 * Index defragmentation — compacts the index node table by
 * eliminating gaps from freed/tombstoned nodes and rebuilding
 * the key-range coverage for optimal lookup performance.
 */

typedef struct {
    uint16_t nodes_before;
    uint16_t nodes_after;
    uint16_t tombstones_removed;
    uint16_t gaps_closed;
    float    fragmentation_before;
    float    fragmentation_after;
} tmk_defrag_result_t;

int tmk_index_defrag(tmk_context_t *ctx, tmk_defrag_result_t *result);
float tmk_index_fragmentation(tmk_context_t *ctx);
int tmk_index_compact_ranges(tmk_context_t *ctx);
int tmk_index_rebuild_depth(tmk_context_t *ctx);
int tmk_index_count_active(tmk_context_t *ctx);
int tmk_index_count_tombstones(tmk_context_t *ctx);
int tmk_index_coverage_gaps(tmk_context_t *ctx, uint16_t partition_id);

#endif
