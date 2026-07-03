#ifndef TMK_SESSION_TABLE_H
#define TMK_SESSION_TABLE_H

#include "../../include/tidemark/types.h"

/*
 * Session table utilities — occupancy tracking, enumeration, and
 * generation-aware session lookup for cross-subsystem coordination.
 */

int tmk_session_table_count_active(tmk_context_t *ctx);
int tmk_session_table_find_by_epoch(tmk_context_t *ctx, uint32_t epoch,
                                    tmk_session_t **out, int max);
int tmk_session_table_gc(tmk_context_t *ctx);
int tmk_session_table_compact(tmk_context_t *ctx);
int tmk_session_count_active(tmk_context_t *ctx);
uint32_t tmk_session_oldest_epoch(tmk_context_t *ctx);
uint32_t tmk_session_newest_epoch(tmk_context_t *ctx);
int tmk_session_gc_stale(tmk_context_t *ctx, uint32_t max_age_epochs);
int tmk_session_enumerate_leases(tmk_context_t *ctx, uint32_t session_id,
                                  uint16_t *out_ids, uint16_t max_count,
                                  uint16_t *out_count);
int tmk_session_enumerate_cursors(tmk_context_t *ctx, uint32_t session_id,
                                   uint16_t *out_ids, uint16_t max_count,
                                   uint16_t *out_count);
int tmk_session_revoke_all_leases(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_close_all_cursors(tmk_context_t *ctx, uint32_t session_id);
int tmk_session_validate_integrity(tmk_context_t *ctx, uint32_t session_id);

#endif
