#ifndef TMK_INTERNAL_STATS_H
#define TMK_INTERNAL_STATS_H

#include "../../include/tidemark/types.h"

/*
 * Runtime statistics and diagnostics.
 */

typedef struct {
    /* Frame processing */
    uint64_t frames_processed;
    uint64_t frames_dropped;
    uint64_t bytes_processed;
    uint64_t crc_failures;

    /* Session */
    uint32_t sessions_opened;
    uint32_t sessions_closed;

    /* Segment */
    uint32_t segments_created;
    uint32_t segments_freed;
    uint64_t bytes_written;
    uint64_t bytes_read;

    /* Compaction */
    uint32_t sweeps_executed;
    uint32_t segments_swept;
    uint32_t tombstones_swept;
    uint32_t merges_completed;
    uint32_t deferred_drains;

    /* Retention */
    uint32_t retention_evaluations;
    uint32_t retention_expirations;
    uint32_t channels_evicted;

    /* Lease */
    uint32_t leases_acquired;
    uint32_t leases_released;
    uint32_t leases_revoked;

    /* Cursor */
    uint32_t cursors_opened;
    uint32_t cursors_closed;
    uint32_t cursor_compactions;

    /* Migration */
    uint32_t migrations_started;
    uint32_t migrations_committed;
    uint32_t segments_transferred;

    /* Epoch */
    uint32_t epoch_advances;
    uint32_t generation_crossings;
    uint32_t cascades_triggered;
} tmk_stats_t;

void tmk_stats_reset(tmk_stats_t *stats);
void tmk_stats_merge(tmk_stats_t *dst, const tmk_stats_t *src);
uint64_t tmk_stats_total_ops(const tmk_stats_t *stats);
float tmk_stats_drop_rate(const tmk_stats_t *stats);
float tmk_stats_sweep_efficiency(const tmk_stats_t *stats);
void tmk_stats_snapshot(const tmk_context_t *ctx, tmk_stats_t *out);

#endif
