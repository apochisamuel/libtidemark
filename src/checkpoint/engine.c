/*
 * libtidemark — checkpoint creation
 *
 * Creates a snapshot manifest of active segments at the current epoch.
 * The manifest stores segment IDs, struct pointers, AND direct data
 * pointers for fast consistency validation during restore.
 *
 * The data_snapshots array captures the raw data pointer from each
 * segment at checkpoint creation time.  This enables the restore
 * path to validate data integrity without re-resolving through
 * the segment struct (which may have been recycled).
 */
#include "engine.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_checkpoint_create(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CHECKPOINT_REF)
        return TMK_ERR_BAD_SUBREC_KIND;

    int slot = -1;
    for (int i = 0; i < TMK_MAX_CHECKPOINTS; i++) {
        if (!ctx->checkpoints[i].active) { slot = i; break; }
    }
    if (slot < 0) return TMK_ERR_CHECKPOINT_FULL;

    ctx->checkpoints[slot].id = frame->header.route_id;
    ctx->checkpoints[slot].active = true;
    ctx->checkpoints[slot].epoch = ctx->global_epoch;
    ctx->checkpoints[slot].segment_count = 0;
    ctx->checkpoints[slot].generation = ctx->global_generation;

    /* Capture all active segments — including data pointers */
    for (int i = 0; i < TMK_MAX_SEGMENTS; i++) {
        if (ctx->segments[i].occupied &&
            ctx->segments[i].phase == TMK_PHASE_ACTIVE) {
            uint16_t idx = ctx->checkpoints[slot].segment_count;
            if (idx < TMK_MAX_SEGMENTS) {
                ctx->checkpoints[slot].segment_ids[idx] = ctx->segments[i].id;
                ctx->checkpoints[slot].segment_refs[idx] = &ctx->segments[i];

                /*
                 * Capture data pointer snapshot.
                 *
                 * This is the ALIAS for deferred_segment_free:
                 * the data pointer is captured at checkpoint time.
                 * If the segment's data buffer is later freed by
                 * compaction sweep (triggered by epoch cascade),
                 * this snapshot pointer becomes dangling.
                 *
                 * No coordination protocol exists between checkpoint/
                 * and compact/ to invalidate these snapshots.
                 */
                ctx->checkpoints[slot].data_snapshots[idx] = ctx->segments[i].data;
                ctx->checkpoints[slot].snapshot_sizes[idx] = ctx->segments[i].used;
                ctx->checkpoints[slot].segment_count++;
            }
        }
    }

    return TMK_OK;
}
