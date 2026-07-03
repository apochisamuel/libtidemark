/*
 * libtidemark — incremental checkpointing
 *
 * Tracks segment modifications since the last checkpoint and
 * captures only the delta.  An incremental checkpoint references
 * a base checkpoint; restoration requires applying the delta
 * on top of the base.
 *
 * A modification is detected by comparing segment generation
 * counters against the base checkpoint's epoch.  Any segment
 * whose create_epoch or generation is newer than the base is
 * included in the incremental set.
 */
#include "incremental.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_checkpoint_find_modified(tmk_context_t *ctx,
                                 uint32_t since_epoch,
                                 uint16_t *out_ids,
                                 uint16_t max_count,
                                 uint16_t *out_count) {
    if (!out_ids || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    for (int i = 0; i < TMK_MAX_SEGMENTS && *out_count < max_count; i++) {
        if (!ctx->segments[i].occupied) continue;
        if (ctx->segments[i].phase != TMK_PHASE_ACTIVE) continue;

        /* Segment is modified if it was created or updated after since_epoch */
        if (ctx->segments[i].create_epoch > since_epoch ||
            ctx->segments[i].generation > since_epoch) {
            out_ids[*out_count] = ctx->segments[i].id;
            (*out_count)++;
        }
    }

    return TMK_OK;
}

int tmk_checkpoint_incremental(tmk_context_t *ctx,
                               uint16_t base_checkpoint_id,
                               tmk_incremental_checkpoint_t *out) {
    if (!out) return TMK_ERR_SHORT_BUF;
    memset(out, 0, sizeof(*out));

    tmk_checkpoint_t *base = tmk_find_checkpoint(ctx, base_checkpoint_id);
    if (!base || !base->active) return TMK_ERR_NOT_FOUND;

    out->base_checkpoint_id = base_checkpoint_id;
    out->base_epoch = base->epoch;
    out->current_epoch = ctx->global_epoch;

    /* Find segments modified since base checkpoint */
    int rc = tmk_checkpoint_find_modified(ctx, base->epoch,
                                          out->modified_segment_ids,
                                          TMK_MAX_SEGMENTS,
                                          &out->modified_count);
    if (rc < 0) return rc;

    /* Calculate bytes saved vs full checkpoint */
    size_t full_size = 0, delta_size = 0;
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied && ctx->segments[i].phase == TMK_PHASE_ACTIVE) {
            full_size += ctx->segments[i].used;
        }
    }
    for (uint16_t i = 0; i < out->modified_count; i++) {
        tmk_segment_t *seg = tmk_find_segment(ctx, out->modified_segment_ids[i]);
        if (seg && seg->occupied) {
            delta_size += seg->used;
        }
    }

    out->bytes_saved = (uint32_t)(full_size > delta_size ?
                                  full_size - delta_size : 0);

    return TMK_OK;
}

int tmk_checkpoint_compute_delta_size(tmk_context_t *ctx,
                                     const tmk_incremental_checkpoint_t *incr,
                                     size_t *out_size) {
    if (!incr || !out_size) return TMK_ERR_SHORT_BUF;
    *out_size = 0;

    for (uint16_t i = 0; i < incr->modified_count; i++) {
        tmk_segment_t *seg = tmk_find_segment(ctx, incr->modified_segment_ids[i]);
        if (seg && seg->occupied) {
            *out_size += seg->used;
        }
    }

    return TMK_OK;
}

bool tmk_checkpoint_is_incremental_worthwhile(tmk_context_t *ctx,
                                              uint16_t base_checkpoint_id) {
    tmk_incremental_checkpoint_t incr;
    int rc = tmk_checkpoint_incremental(ctx, base_checkpoint_id, &incr);
    if (rc < 0) return false;

    /* Incremental is worthwhile if we save > 50% of data */
    tmk_checkpoint_t *base = tmk_find_checkpoint(ctx, base_checkpoint_id);
    if (!base) return false;

    return incr.modified_count < base->segment_count / 2;
}
