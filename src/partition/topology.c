/*
 * libtidemark — partition topology analysis
 *
 * Provides comprehensive analysis of partition state:
 * - Per-partition statistics (utilization, fragmentation, coverage)
 * - Global topology metrics (balance, utilization, connectivity)
 * - Hotspot and cold-spot detection for migration planning
 *
 * Utilization: ratio of used bytes to total capacity across segments.
 * Fragmentation: ratio of gaps in segment ID sequence.
 * Balance coefficient: coefficient of variation of segment counts.
 * Connectivity: which partitions share channels (for migration deps).
 */
#include "topology.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>
#include <math.h>

int tmk_partition_stats(tmk_context_t *ctx, uint16_t partition_id,
                        tmk_partition_stats_t *stats) {
    if (!stats) return TMK_ERR_SHORT_BUF;
    memset(stats, 0, sizeof(*stats));

    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    stats->partition_id = partition_id;
    stats->segment_count = part->segment_count;
    stats->channel_count = part->channel_count;
    stats->create_epoch = part->create_epoch;
    stats->generation = part->generation;

    /* Aggregate segment statistics */
    for (uint16_t i = 0; i < part->segment_count; i++) {
        tmk_segment_t *seg = tmk_find_segment(ctx, part->segment_ids[i]);
        if (!seg || !seg->occupied) continue;

        stats->total_bytes += (uint32_t)seg->capacity;
        stats->total_used += (uint32_t)seg->used;

        if (seg->tombstone) stats->has_tombstones = true;
    }

    /* Utilization */
    stats->utilization = (stats->total_bytes > 0) ?
        (float)stats->total_used / (float)stats->total_bytes : 0.0f;

    /* Fragmentation: gaps in segment ID sequence */
    if (part->segment_count >= 2) {
        uint16_t min_id = UINT16_MAX, max_id = 0;
        for (uint16_t i = 0; i < part->segment_count; i++) {
            if (part->segment_ids[i] < min_id) min_id = part->segment_ids[i];
            if (part->segment_ids[i] > max_id) max_id = part->segment_ids[i];
        }
        uint16_t span = max_id - min_id + 1;
        uint16_t gaps = span - part->segment_count;
        stats->fragmentation = (float)gaps / (float)span;
    }

    /* Count index nodes covering this partition */
    stats->index_coverage = 0;
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (ctx->index_nodes[i].active &&
            ctx->index_nodes[i].partition_id == partition_id) {
            stats->index_coverage++;
        }
    }

    /* Count active leases for segments in this partition */
    stats->active_leases = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (!ctx->leases[i].active) continue;
        tmk_segment_t *seg = tmk_find_segment(ctx, ctx->leases[i].segment_id);
        if (seg && seg->partition_id == partition_id) {
            stats->active_leases++;
        }
    }

    /* Count active cursors for segments in this partition */
    stats->active_cursors = 0;
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (!ctx->cursors[i].active) continue;
        if (ctx->cursors[i].partition_id == partition_id) {
            stats->active_cursors++;
        }
    }

    /* Check if this partition is a migration target */
    stats->migration_target = false;
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active &&
            ctx->migrations[i].dest_partition_id == partition_id) {
            stats->migration_target = true;
            break;
        }
    }

    return TMK_OK;
}

int tmk_partition_analyze_topology(tmk_context_t *ctx,
                                   tmk_topology_report_t *report) {
    if (!report) return TMK_ERR_SHORT_BUF;
    memset(report, 0, sizeof(*report));

    double sum_segs = 0, sum_segs_sq = 0;
    int active_count = 0;

    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (!ctx->partitions[i].active) continue;

        tmk_partition_stats_t *ps = &report->partitions[report->count];
        tmk_partition_stats(ctx, ctx->partitions[i].id, ps);

        report->total_segments += ps->segment_count;
        report->total_bytes += ps->total_bytes;
        report->total_used += ps->total_used;

        if (ps->segment_count == 0) report->empty_partitions++;
        if (ps->segment_count > TMK_MAX_SEGMENTS / TMK_MAX_PARTITIONS * 2)
            report->overloaded_partitions++;

        double sc = (double)ps->segment_count;
        sum_segs += sc;
        sum_segs_sq += sc * sc;
        active_count++;

        report->count++;
    }

    /* Global utilization */
    report->global_utilization = (report->total_bytes > 0) ?
        (float)report->total_used / (float)report->total_bytes : 0.0f;

    /* Balance coefficient (CV of segment counts) */
    if (active_count >= 2) {
        double mean = sum_segs / active_count;
        double var = (sum_segs_sq / active_count) - (mean * mean);
        double stddev = sqrt(var > 0 ? var : 0);
        report->balance_coefficient = (mean > 0) ?
            (float)(stddev / mean) : 0.0f;
    }

    return TMK_OK;
}

int tmk_partition_find_hotspots(tmk_context_t *ctx,
                                uint16_t *out_ids, uint16_t max_count,
                                uint16_t *out_count) {
    if (!out_ids || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    /* Hotspot: partition with utilization > 80% or many active leases */
    for (int i = 0; i < TMK_MAX_PARTITIONS && *out_count < max_count; i++) {
        if (!ctx->partitions[i].active) continue;

        tmk_partition_stats_t stats;
        tmk_partition_stats(ctx, ctx->partitions[i].id, &stats);

        if (stats.utilization > 0.8f || stats.active_leases > 4 ||
            stats.active_cursors > 4) {
            out_ids[*out_count] = ctx->partitions[i].id;
            (*out_count)++;
        }
    }

    return TMK_OK;
}

int tmk_partition_find_cold(tmk_context_t *ctx,
                            uint16_t *out_ids, uint16_t max_count,
                            uint16_t *out_count) {
    if (!out_ids || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    for (int i = 0; i < TMK_MAX_PARTITIONS && *out_count < max_count; i++) {
        if (!ctx->partitions[i].active) continue;

        tmk_partition_stats_t stats;
        tmk_partition_stats(ctx, ctx->partitions[i].id, &stats);

        if (stats.utilization < 0.2f && stats.active_leases == 0 &&
            stats.active_cursors == 0) {
            out_ids[*out_count] = ctx->partitions[i].id;
            (*out_count)++;
        }
    }

    return TMK_OK;
}

float tmk_partition_balance_score(tmk_context_t *ctx) {
    tmk_topology_report_t report;
    tmk_partition_analyze_topology(ctx, &report);
    /* Invert: 0.0 = perfectly balanced, higher = more imbalanced */
    return 1.0f - (report.balance_coefficient > 1.0f ?
                   0.0f : 1.0f - report.balance_coefficient);
}

int tmk_partition_connectivity_matrix(tmk_context_t *ctx,
                                      uint8_t *matrix, uint16_t dim) {
    if (!matrix || dim < TMK_MAX_PARTITIONS) return TMK_ERR_SHORT_BUF;
    memset(matrix, 0, (size_t)dim * dim);

    /* Two partitions are "connected" if they share a channel
     * (i.e., the same channel has been bound to both at some point) */
    for (int c = 0; c < TMK_MAX_CHANNELS; c++) {
        if (!ctx->channels[c].active) continue;
        uint16_t part_a = ctx->channels[c].bound_partition_id;
        if (part_a == 0) continue;

        /* Find other channels bound to different partitions in same session */
        for (int d = c + 1; d < TMK_MAX_CHANNELS; d++) {
            if (!ctx->channels[d].active) continue;
            if (ctx->channels[d].session_id != ctx->channels[c].session_id)
                continue;
            uint16_t part_b = ctx->channels[d].bound_partition_id;
            if (part_b == 0 || part_b == part_a) continue;

            /* Map partition IDs to indices */
            int idx_a = -1, idx_b = -1;
            for (int p = 0; p < TMK_MAX_PARTITIONS; p++) {
                if (ctx->partitions[p].active) {
                    if (ctx->partitions[p].id == part_a) idx_a = p;
                    if (ctx->partitions[p].id == part_b) idx_b = p;
                }
            }

            if (idx_a >= 0 && idx_b >= 0 &&
                idx_a < dim && idx_b < dim) {
                matrix[idx_a * dim + idx_b] = 1;
                matrix[idx_b * dim + idx_a] = 1;
            }
        }
    }

    return TMK_OK;
}
