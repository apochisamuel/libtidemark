#ifndef TMK_PARTITION_TOPOLOGY_H
#define TMK_PARTITION_TOPOLOGY_H

#include "../../include/tidemark/types.h"

/*
 * Partition topology analysis — examines cross-partition relationships,
 * segment distribution, and connectivity patterns.
 */

typedef struct {
    uint16_t partition_id;
    uint16_t segment_count;
    uint16_t channel_count;
    uint32_t total_bytes;
    uint32_t total_used;
    uint32_t create_epoch;
    uint32_t generation;
    float    utilization;
    float    fragmentation;
    uint16_t index_coverage;
    uint16_t active_leases;
    uint16_t active_cursors;
    bool     has_tombstones;
    bool     migration_target;
} tmk_partition_stats_t;

typedef struct {
    tmk_partition_stats_t partitions[TMK_MAX_PARTITIONS];
    uint16_t count;
    float    global_utilization;
    float    balance_coefficient;
    uint32_t total_segments;
    uint32_t total_bytes;
    uint32_t total_used;
    uint16_t empty_partitions;
    uint16_t overloaded_partitions;
} tmk_topology_report_t;

int tmk_partition_analyze_topology(tmk_context_t *ctx,
                                   tmk_topology_report_t *report);
int tmk_partition_stats(tmk_context_t *ctx, uint16_t partition_id,
                        tmk_partition_stats_t *stats);
int tmk_partition_find_hotspots(tmk_context_t *ctx,
                                uint16_t *out_ids, uint16_t max_count,
                                uint16_t *out_count);
int tmk_partition_find_cold(tmk_context_t *ctx,
                            uint16_t *out_ids, uint16_t max_count,
                            uint16_t *out_count);
float tmk_partition_balance_score(tmk_context_t *ctx);
int tmk_partition_connectivity_matrix(tmk_context_t *ctx,
                                      uint8_t *matrix, uint16_t dim);

#endif
