/*
 * libtidemark — session state snapshot
 *
 * Captures a consistent view of all objects associated with a session.
 * Used for debugging, diagnostics, and state-transfer during migration.
 *
 * Validation checks for orphaned objects: cursors without valid segments,
 * leases without active sessions, channels bound to freed partitions.
 */
#include "snapshot.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_session_snapshot(tmk_context_t *ctx, uint32_t session_id,
                         tmk_session_snapshot_t *out) {
    if (!ctx || !out) return TMK_ERR_SHORT_BUF;
    memset(out, 0, sizeof(*out));

    tmk_session_t *sess = tmk_find_session(ctx, session_id);
    if (!sess) return TMK_ERR_NOT_FOUND;

    out->session_id = sess->id;
    out->epoch = sess->epoch;
    out->generation = ctx->global_generation;
    out->phase = sess->phase;

    /* Enumerate channels */
    out->channel_count = 0;
    for (int i = 0; i < TMK_MAX_CHANNELS; i++) {
        if (ctx->channels[i].active &&
            ctx->channels[i].session_id == session_id &&
            out->channel_count < TMK_MAX_CHANNELS) {
            out->channel_ids[out->channel_count++] = ctx->channels[i].id;
        }
    }

    /* Enumerate leases */
    out->lease_count = 0;
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (ctx->leases[i].active &&
            ctx->leases[i].session_id == session_id &&
            out->lease_count < TMK_MAX_LEASES) {
            out->lease_ids[out->lease_count++] = ctx->leases[i].id;
        }
    }

    /* Enumerate cursors */
    out->cursor_count = 0;
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active &&
            ctx->cursors[i].session_id == session_id &&
            out->cursor_count < TMK_MAX_CURSORS) {
            out->cursor_ids[out->cursor_count++] = ctx->cursors[i].id;
        }
    }

    /* Count segments and bytes */
    out->segment_count = 0;
    out->total_segment_bytes = 0;
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied) {
            out->segment_count++;
            out->total_segment_bytes += (uint32_t)ctx->segments[i].used;
        }
    }

    /* Count partitions */
    out->partition_count = 0;
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (ctx->partitions[i].active) out->partition_count++;
    }

    /* Check for active migration/checkpoint */
    out->has_active_migration = false;
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active) {
            out->has_active_migration = true;
            break;
        }
    }
    out->has_active_checkpoint = false;
    for (int i = 0; i < TMK_MAX_CHECKPOINTS; i++) {
        if (ctx->checkpoints[i].active) {
            out->has_active_checkpoint = true;
            break;
        }
    }

    out->deferred_count = ctx->deferred_count;
    out->tombstone_count = ctx->tombstone_count;

    return TMK_OK;
}

int tmk_session_snapshot_validate(tmk_context_t *ctx,
                                  const tmk_session_snapshot_t *snap) {
    if (!ctx || !snap) return TMK_ERR_SHORT_BUF;

    int issues = 0;

    /* Check cursors reference valid segments */
    for (uint16_t i = 0; i < snap->cursor_count; i++) {
        tmk_cursor_t *cur = tmk_find_cursor(ctx, snap->cursor_ids[i]);
        if (!cur || !cur->active) { issues++; continue; }

        tmk_segment_t *seg = tmk_find_segment(ctx, cur->segment_id);
        if (!seg || !seg->occupied) {
            issues++; /* orphaned cursor */
        }
    }

    /* Check leases reference valid segments */
    for (uint16_t i = 0; i < snap->lease_count; i++) {
        tmk_lease_t *lease = tmk_find_lease(ctx, snap->lease_ids[i]);
        if (!lease || !lease->active) { issues++; continue; }

        if (lease->segment_id > 0) {
            tmk_segment_t *seg = tmk_find_segment(ctx, lease->segment_id);
            if (!seg || !seg->occupied) {
                issues++; /* orphaned lease */
            }
        }
    }

    /* Check channels reference valid partitions */
    for (uint16_t i = 0; i < snap->channel_count; i++) {
        tmk_channel_t *ch = tmk_find_channel(ctx, snap->channel_ids[i]);
        if (!ch || !ch->active) { issues++; continue; }

        if (ch->bound_partition_id > 0) {
            tmk_partition_t *part = tmk_find_partition(ctx, ch->bound_partition_id);
            if (!part || !part->active) {
                issues++; /* orphaned channel binding */
            }
        }
    }

    return issues;
}

int tmk_session_snapshot_diff(const tmk_session_snapshot_t *before,
                              const tmk_session_snapshot_t *after,
                              tmk_session_snapshot_t *delta) {
    if (!before || !after || !delta) return TMK_ERR_SHORT_BUF;
    memset(delta, 0, sizeof(*delta));

    delta->session_id = after->session_id;
    delta->epoch = after->epoch - before->epoch;
    delta->channel_count = (after->channel_count > before->channel_count) ?
        after->channel_count - before->channel_count : 0;
    delta->lease_count = (after->lease_count > before->lease_count) ?
        after->lease_count - before->lease_count : 0;
    delta->cursor_count = (after->cursor_count > before->cursor_count) ?
        after->cursor_count - before->cursor_count : 0;
    delta->segment_count = (after->segment_count > before->segment_count) ?
        after->segment_count - before->segment_count : 0;

    int32_t byte_diff = (int32_t)after->total_segment_bytes -
                        (int32_t)before->total_segment_bytes;
    delta->total_segment_bytes = (byte_diff > 0) ? (uint32_t)byte_diff : 0;

    return TMK_OK;
}
