/*
 * libtidemark — index defragmentation
 *
 * The index table can become fragmented as nodes are created and
 * tombstoned over time.  Defragmentation:
 *
 * 1. Removes all tombstoned nodes
 * 2. Compacts active nodes to fill gaps
 * 3. Recalculates key-range coverage
 * 4. Rebuilds depth counters
 *
 * Fragmentation metric: ratio of inactive slots within the range
 * of active slots.  0.0 = perfect, 1.0 = completely fragmented.
 */
#include "defrag.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_index_count_active(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active) count++;
    }
    return count;
}

int tmk_index_count_tombstones(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active && ctx->index_nodes[i].tombstone)
            count++;
    }
    return count;
}

float tmk_index_fragmentation(tmk_context_t *ctx) {
    int first_active = -1, last_active = -1;
    int active = 0;

    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active && !ctx->index_nodes[i].tombstone) {
            if (first_active < 0) first_active = i;
            last_active = i;
            active++;
        }
    }

    if (active <= 1 || first_active < 0) return 0.0f;

    int span = last_active - first_active + 1;
    int gaps = span - active;

    return (float)gaps / (float)span;
}

int tmk_index_defrag(tmk_context_t *ctx, tmk_defrag_result_t *result) {
    if (!result) return TMK_ERR_SHORT_BUF;
    memset(result, 0, sizeof(*result));

    result->fragmentation_before = tmk_index_fragmentation(ctx);
    result->nodes_before = (uint16_t)tmk_index_count_active(ctx);

    /* Step 1: remove tombstones */
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active && ctx->index_nodes[i].tombstone) {
            ctx->index_nodes[i].active = false;
            ctx->index_nodes[i].tombstone = false;
            ctx->index_nodes[i].segment_ref = NULL;
            ctx->index_nodes[i].partition_ref = NULL;
            ctx->index_nodes[i].cursor_ref = NULL;
            result->tombstones_removed++;
        }
    }

    /* Step 2: compact — move active nodes to fill gaps */
    int write_idx = 0;
    for (int read_idx = 0; read_idx < TMK_MAX_INDEX_NODES; read_idx++) {
        if (ctx->index_nodes[read_idx].active) {
            if (write_idx != read_idx) {
                ctx->index_nodes[write_idx] = ctx->index_nodes[read_idx];
                ctx->index_nodes[read_idx].active = false;
                ctx->index_nodes[read_idx].segment_ref = NULL;
                ctx->index_nodes[read_idx].partition_ref = NULL;
                ctx->index_nodes[read_idx].cursor_ref = NULL;
                result->gaps_closed++;
            }
            write_idx++;
        }
    }

    /* Step 3: rebuild depth counters */
    tmk_index_rebuild_depth(ctx);

    result->nodes_after = (uint16_t)tmk_index_count_active(ctx);
    result->fragmentation_after = tmk_index_fragmentation(ctx);

    return TMK_OK;
}

int tmk_index_compact_ranges(tmk_context_t *ctx) {
    /* Merge adjacent nodes with contiguous key ranges */
    int merged = 0;

    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (!ctx->index_nodes[i].active) continue;

        for (int j = i + 1; j < TMK_MAX_INDEX_NODES; j++) {
            if (!ctx->index_nodes[j].active) continue;

            /* Check if ranges are adjacent */
            if (ctx->index_nodes[i].key_max + 1 ==
                ctx->index_nodes[j].key_min &&
                ctx->index_nodes[i].partition_id ==
                ctx->index_nodes[j].partition_id &&
                ctx->index_nodes[i].segment_id ==
                ctx->index_nodes[j].segment_id) {

                /* Merge j into i */
                ctx->index_nodes[i].key_max = ctx->index_nodes[j].key_max;
                ctx->index_nodes[i].child_count +=
                    ctx->index_nodes[j].child_count;
                ctx->index_nodes[j].active = false;
                merged++;
                break;
            }
        }
    }

    return merged;
}

int tmk_index_rebuild_depth(tmk_context_t *ctx) {
    /* Calculate depth based on partition grouping:
     * depth 0 = covers single segment
     * depth 1 = covers single partition
     * depth 2 = covers multiple partitions */
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (!ctx->index_nodes[i].active) continue;

        uint16_t part_id = ctx->index_nodes[i].partition_id;
        bool cross_partition = false;

        /* Check if any other node in same partition has different segment */
        for (int j = 0; j < TMK_MAX_INDEX_NODES; j++) {
            if (j == i || !ctx->index_nodes[j].active) continue;
            if (ctx->index_nodes[j].partition_id == part_id &&
                ctx->index_nodes[j].segment_id != ctx->index_nodes[i].segment_id) {
                ctx->index_nodes[i].depth = 1;
                break;
            }
            if (ctx->index_nodes[j].partition_id != part_id) {
                cross_partition = true;
            }
        }

        if (cross_partition && ctx->index_nodes[i].partition_ref) {
            ctx->index_nodes[i].depth = 2;
        }
    }

    return TMK_OK;
}

int tmk_index_coverage_gaps(tmk_context_t *ctx, uint16_t partition_id) {
    /* Count key-range gaps within a partition's index nodes */
    uint32_t ranges[TMK_MAX_INDEX_NODES][2];
    int range_count = 0;

    for (int i = 0; i < TMK_MAX_INDEX_NODES && range_count < TMK_MAX_INDEX_NODES; i++) {
        if (!ctx->index_nodes[i].active) continue;
        if (ctx->index_nodes[i].partition_id != partition_id) continue;

        ranges[range_count][0] = ctx->index_nodes[i].key_min;
        ranges[range_count][1] = ctx->index_nodes[i].key_max;
        range_count++;
    }

    if (range_count < 2) return 0;

    /* Sort ranges by key_min (insertion sort) */
    for (int i = 1; i < range_count; i++) {
        uint32_t tmp_min = ranges[i][0];
        uint32_t tmp_max = ranges[i][1];
        int j = i;
        while (j > 0 && ranges[j-1][0] > tmp_min) {
            ranges[j][0] = ranges[j-1][0];
            ranges[j][1] = ranges[j-1][1];
            j--;
        }
        ranges[j][0] = tmp_min;
        ranges[j][1] = tmp_max;
    }

    /* Count gaps between consecutive ranges */
    int gaps = 0;
    for (int i = 0; i + 1 < range_count; i++) {
        if (ranges[i][1] + 1 < ranges[i+1][0]) {
            gaps++;
        }
    }

    return gaps;
}
