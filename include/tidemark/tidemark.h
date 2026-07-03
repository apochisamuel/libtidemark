/*
 * libtidemark — public API
 *
 * Segmented time-series storage engine for embedded/edge deployments.
 * Manages partitioned time-series data through a compact binary wire
 * protocol with multi-channel ingestion, segment compaction, lease-based
 * read isolation, cross-partition migration, and checkpoint recovery.
 */
#ifndef TMK_TIDEMARK_H
#define TMK_TIDEMARK_H

#include "types.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Context lifecycle ──────────────────────────────────── */
tmk_context_t *tmk_context_create(void);
void           tmk_context_destroy(tmk_context_t *ctx);

/* ── Ingestion APIs (one per harness) ───────────────────── */
int tmk_channel_ingest(tmk_context_t *ctx, const uint8_t *buf, size_t len);
int tmk_session_process(tmk_context_t *ctx, const uint8_t *buf, size_t len);
int tmk_stream_execute(tmk_context_t *ctx, const uint8_t *buf, size_t len);
int tmk_migrate_process(tmk_context_t *ctx, const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* TMK_TIDEMARK_H */

/* ═══════════════════════════════════════════════════════════
 * Extended API — utility and diagnostic functions
 * ═══════════════════════════════════════════════════════════ */

/*
 * Session management
 */
int tmk_session_close_graceful(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_force_close(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_health_check(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_count_active(tmk_context_t *ctx);
uint32_t tmk_session_oldest_epoch(tmk_context_t *ctx);
uint32_t tmk_session_newest_epoch(tmk_context_t *ctx);
int tmk_session_gc_stale(tmk_context_t *ctx, uint32_t max_age_epochs);

/*
 * Partition management
 */
int tmk_partition_count_active(tmk_context_t *ctx);
int tmk_partition_total_segments(tmk_context_t *ctx);
int tmk_partition_total_bytes(tmk_context_t *ctx);
uint16_t tmk_partition_largest(tmk_context_t *ctx);
uint16_t tmk_partition_smallest(tmk_context_t *ctx);
bool tmk_partition_has_active_leases(tmk_context_t *ctx, uint16_t partition_id);
bool tmk_partition_has_active_cursors(tmk_context_t *ctx, uint16_t partition_id);
float tmk_partition_balance_score(tmk_context_t *ctx);

/*
 * Segment operations
 */
int tmk_segment_count_records(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_verify_checksum(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_scrub(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_distinct_keys(const uint8_t *data, size_t len);

/*
 * Index management
 */
int tmk_index_count_active(tmk_context_t *ctx);
int tmk_index_count_tombstones(tmk_context_t *ctx);
float tmk_index_fragmentation(tmk_context_t *ctx);

/*
 * Cursor operations
 */
bool tmk_cursor_at_end(tmk_context_t *ctx, uint16_t cursor_id);

/*
 * Lease queries
 */
int tmk_lease_count_active(tmk_context_t *ctx);

/*
 * Migration planning
 */
bool tmk_migrate_should_rebalance(tmk_context_t *ctx);

/*
 * Checkpoint operations
 */
bool tmk_checkpoint_is_incremental_worthwhile(tmk_context_t *ctx,
                                              uint16_t base_checkpoint_id);

/*
 * Protocol negotiation
 */
typedef struct tmk_negotiate_offer tmk_negotiate_offer_t_api;
typedef struct tmk_negotiate_result tmk_negotiate_result_t_api;

/*
 * Wire diagnostics
 */
typedef struct tmk_diagnostic_report tmk_diagnostic_report_api_t;

