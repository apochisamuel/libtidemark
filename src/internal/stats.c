/*
 * libtidemark — runtime statistics
 *
 * Collects operational metrics for monitoring and diagnostics.
 * Statistics are gathered from context state snapshots rather than
 * inline instrumentation to minimize overhead on hot paths.
 */
#include "stats.h"
#include <string.h>

void tmk_stats_reset(tmk_stats_t *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
}

void tmk_stats_merge(tmk_stats_t *dst, const tmk_stats_t *src) {
    if (!dst || !src) return;

    dst->frames_processed += src->frames_processed;
    dst->frames_dropped += src->frames_dropped;
    dst->bytes_processed += src->bytes_processed;
    dst->crc_failures += src->crc_failures;

    dst->sessions_opened += src->sessions_opened;
    dst->sessions_closed += src->sessions_closed;

    dst->segments_created += src->segments_created;
    dst->segments_freed += src->segments_freed;
    dst->bytes_written += src->bytes_written;
    dst->bytes_read += src->bytes_read;

    dst->sweeps_executed += src->sweeps_executed;
    dst->segments_swept += src->segments_swept;
    dst->tombstones_swept += src->tombstones_swept;
    dst->merges_completed += src->merges_completed;
    dst->deferred_drains += src->deferred_drains;

    dst->retention_evaluations += src->retention_evaluations;
    dst->retention_expirations += src->retention_expirations;
    dst->channels_evicted += src->channels_evicted;

    dst->leases_acquired += src->leases_acquired;
    dst->leases_released += src->leases_released;
    dst->leases_revoked += src->leases_revoked;

    dst->cursors_opened += src->cursors_opened;
    dst->cursors_closed += src->cursors_closed;
    dst->cursor_compactions += src->cursor_compactions;

    dst->migrations_started += src->migrations_started;
    dst->migrations_committed += src->migrations_committed;
    dst->segments_transferred += src->segments_transferred;

    dst->epoch_advances += src->epoch_advances;
    dst->generation_crossings += src->generation_crossings;
    dst->cascades_triggered += src->cascades_triggered;
}

uint64_t tmk_stats_total_ops(const tmk_stats_t *stats) {
    if (!stats) return 0;
    return stats->frames_processed + stats->frames_dropped;
}

float tmk_stats_drop_rate(const tmk_stats_t *stats) {
    if (!stats) return 0.0f;
    uint64_t total = tmk_stats_total_ops(stats);
    if (total == 0) return 0.0f;
    return (float)stats->frames_dropped / (float)total;
}

float tmk_stats_sweep_efficiency(const tmk_stats_t *stats) {
    if (!stats || stats->sweeps_executed == 0) return 0.0f;
    return (float)stats->segments_swept / (float)stats->sweeps_executed;
}

void tmk_stats_snapshot(const tmk_context_t *ctx, tmk_stats_t *out) {
    if (!ctx || !out) return;
    memset(out, 0, sizeof(*out));

    /* Count active objects */
    for (int i = 0; i < TMK_MAX_SESSIONS; i++) {
        if (ctx->sessions[i].active) out->sessions_opened++;
    }
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied) out->segments_created++;
    }
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active) out->leases_acquired++;
    }
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active) out->cursors_opened++;
    }

    out->epoch_advances = ctx->global_epoch;
    out->generation_crossings = ctx->global_generation;
}
