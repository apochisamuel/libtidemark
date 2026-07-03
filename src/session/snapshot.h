#ifndef TMK_SESSION_SNAPSHOT_H
#define TMK_SESSION_SNAPSHOT_H

#include "../../include/tidemark/types.h"

/*
 * Session state snapshot for debugging and diagnostics.
 * Captures a complete picture of all active objects within a session.
 */

typedef struct {
    uint32_t session_id;
    uint32_t epoch;
    uint32_t generation;
    tmk_phase_t phase;

    uint16_t channel_count;
    uint16_t channel_ids[TMK_MAX_CHANNELS];

    uint16_t lease_count;
    uint16_t lease_ids[TMK_MAX_LEASES];

    uint16_t cursor_count;
    uint16_t cursor_ids[TMK_MAX_CURSORS];

    uint32_t total_segment_bytes;
    uint16_t segment_count;
    uint16_t partition_count;

    bool     has_active_migration;
    bool     has_active_checkpoint;
    uint16_t deferred_count;
    uint16_t tombstone_count;
} tmk_session_snapshot_t;

int tmk_session_snapshot(tmk_context_t *ctx, uint32_t session_id,
                         tmk_session_snapshot_t *out);
int tmk_session_snapshot_validate(tmk_context_t *ctx,
                                  const tmk_session_snapshot_t *snap);
int tmk_session_snapshot_diff(const tmk_session_snapshot_t *before,
                              const tmk_session_snapshot_t *after,
                              tmk_session_snapshot_t *delta);

#endif
