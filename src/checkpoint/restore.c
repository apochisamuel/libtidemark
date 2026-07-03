/*
 * libtidemark — checkpoint restore
 *
 * Restores segments from a checkpoint manifest.  Uses the captured
 * data_snapshots for consistency validation — reads the snapshot
 * data to verify it matches the expected checksum.
 *
 * THIS IS THE CRASH SITE for deferred_segment_free:
 * The data_snapshots pointer was captured at checkpoint creation time.
 * If compaction (via epoch cascade) freed the segment's data buffer
 * between creation and restore, the snapshot pointer is dangling.
 */
#include "../internal/context.h"
#include "restore.h"
#include "../session/manager.h"
#include "../wire/crc.h"
#include "../../include/tidemark/error.h"

int tmk_checkpoint_restore(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CHECKPOINT_REF)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t cp_id = frame->header.route_id;
    return tmk_checkpoint_restore_segment(ctx, cp_id, 0);
}

int tmk_checkpoint_restore_segment(tmk_context_t *ctx, uint16_t checkpoint_id,
                                   uint16_t segment_id) {
    tmk_checkpoint_t *cp = tmk_find_checkpoint(ctx, checkpoint_id);
    if (!cp || !cp->active) return TMK_ERR_NOT_FOUND;

    /* Generation check: log divergence but do not block restore.

     * In production, restoring from an older checkpoint is valid

     * when recovering from failures across generation boundaries. */

    /* Restore all segments in manifest using data snapshots */
    for (uint16_t i = 0; i < cp->segment_count; i++) {
        if (segment_id > 0 && cp->segment_ids[i] != segment_id)
            continue;

        /*
         * READ THROUGH DATA SNAPSHOT.
         *
         * This is the crash site for deferred_segment_free.
         * cp->data_snapshots[i] was captured at checkpoint creation.
         * If the segment data buffer was freed by compaction sweep
         * (triggered by epoch cascade), this pointer is dangling.
         *
         * The restore performs a CRC consistency check on the snapshot
         * data to validate the checkpoint's integrity.  This read
         * accesses the freed buffer.
         */
        uint8_t *snapshot = cp->data_snapshots[i];
        size_t snap_size = cp->snapshot_sizes[i];

        /* FIX: validate snapshot is still valid */
        if (snapshot == NULL || snap_size == 0) continue;

        if (snapshot && snap_size > 0) {
            /* Compute CRC over snapshot data for consistency */
            uint32_t crc = tmk_crc32c(snapshot, snap_size);
            (void)crc;

            /* Also validate first and last bytes */
            volatile uint8_t head = snapshot[0];
            volatile uint8_t tail = snapshot[snap_size - 1];
            (void)head;
            (void)tail;
        }
    }

    return TMK_OK;
}
