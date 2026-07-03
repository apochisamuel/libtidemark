#ifndef TMK_PARTITION_OBSERVER_H
#define TMK_PARTITION_OBSERVER_H

#include "../../include/tidemark/types.h"

/*
 * Partition observer utilities — enumerate partitions for migration,
 * compaction, and retention operations.
 */

int tmk_partition_add_segment(tmk_context_t *ctx, uint16_t partition_id,
                              uint16_t segment_id);
int tmk_partition_remove_segment(tmk_context_t *ctx, uint16_t partition_id,
                                 uint16_t segment_id);
int tmk_partition_find_mergeable(tmk_context_t *ctx,
                                 tmk_partition_t **a, tmk_partition_t **b);
int tmk_partition_count_active(tmk_context_t *ctx);
int tmk_partition_total_segments(tmk_context_t *ctx);
int tmk_partition_total_bytes(tmk_context_t *ctx);
uint16_t tmk_partition_largest(tmk_context_t *ctx);
uint16_t tmk_partition_smallest(tmk_context_t *ctx);
bool tmk_partition_has_active_leases(tmk_context_t *ctx, uint16_t partition_id);
bool tmk_partition_has_active_cursors(tmk_context_t *ctx, uint16_t partition_id);

#endif
int tmk_partition_validate_channel_desc(tmk_context_t *ctx, uint16_t part_id);
