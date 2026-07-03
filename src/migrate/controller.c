/*
 * libtidemark — migration controller
 *
 * Manages cross-partition segment transfer.  Migration is a multi-step
 * process: begin → transfer → commit.  Each step validates ownership
 * tokens and generation counters.
 */
#include "../internal/context.h"
#include "controller.h"
#include "../session/manager.h"
#include "../partition/registry.h"
#include "../../include/tidemark/error.h"

int tmk_migrate_begin(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_MIGRATION_DESC)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Check for existing active migration */
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active) return TMK_ERR_MIGRATION_ACTIVE;
    }

    uint16_t src_id = frame->header.partition_id;
    uint16_t dst_id = frame->header.route_id;

    tmk_partition_t *src = tmk_find_partition(ctx, src_id);
    tmk_partition_t *dst = tmk_find_partition(ctx, dst_id);
    if (!src || !dst) return TMK_ERR_NOT_FOUND;
    if (!src->active || !dst->active) return TMK_ERR_NOT_ACTIVE;

    if (frame->header.owner_token != src->owner_token)
        return TMK_ERR_BAD_OWNER;

    /* Find free migration slot */
    int slot = -1;
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (!ctx->migrations[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_MIGRATION_FULL;

    ctx->migrations[slot].id = frame->header.route_id;
    ctx->migrations[slot].active = true;
    ctx->migrations[slot].source_partition_id = src_id;
    ctx->migrations[slot].dest_partition_id = dst_id;
    ctx->migrations[slot].owner_token = frame->header.owner_token;
    ctx->migrations[slot].generation = ctx->global_generation;
    ctx->migrations[slot].phase = TMK_PHASE_ACTIVE;
    ctx->migrations[slot].transferred_count = 0;

    return TMK_OK;
}

int tmk_migrate_transfer(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_MIGRATION_DESC)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Find active migration */
    tmk_migration_t *mig = NULL;
    for (int i = 0; i < TMK_MAX_MIGRATIONS; i++) {
        if (ctx->migrations[i].active) { mig = &ctx->migrations[i]; break; }
    }
    if (!mig) return TMK_ERR_NOT_FOUND;

    uint16_t seg_id = frame->header.route_id;
    tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;

    /* Record the transfer */
    if (mig->transferred_count < TMK_MAX_SEGMENTS) {
        mig->transferred_segments[mig->transferred_count++] = seg_id;
    }

    /* Move segment from source to destination */
    tmk_partition_remove_segment(ctx, mig->source_partition_id, seg_id);
    tmk_partition_add_segment(ctx, mig->dest_partition_id, seg_id);
    seg->partition_id = mig->dest_partition_id;

    return TMK_OK;
}
