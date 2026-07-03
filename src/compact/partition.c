/*
 * libtidemark — partition merging
 *
 * Merges two low-occupancy partitions into one.  The source partition
 * is freed after its segments are transferred.
 *
 * BUG SURFACE (partition_merge_cursor): Cross-partition index entries
 * that reference the freed partition's metadata are NOT updated.
 * Cursor traversal through stale index entries dereferences freed
 * partition metadata.
 */
#include "../internal/context.h"
#include "partition.h"
#include "../partition/registry.h"
#include "../partition/observer.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_compact_merge_partitions(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_CONTROL)
        return TMK_ERR_BAD_SUBREC_KIND;

    /* Find two mergeable partitions */
    tmk_partition_t *part_a = NULL, *part_b = NULL;
    int rc = tmk_partition_find_mergeable(ctx, &part_a, &part_b);
    if (rc < 0) return rc;
    if (!part_a || !part_b) return TMK_ERR_NOT_FOUND;

    /* Transfer segments from B to A */
    for (uint16_t i = 0; i < part_b->segment_count; i++) {
        uint16_t seg_id = part_b->segment_ids[i];
        tmk_segment_t *seg = tmk_find_segment(ctx, seg_id);
        if (seg) {
            seg->partition_id = part_a->id;
            tmk_partition_add_segment(ctx, part_a->id, seg_id);
        }
    }

    /* Free partition B */
    part_b->active = false;
    part_b->phase = TMK_PHASE_FREED;
    part_b->segment_count = 0;

    return TMK_OK;
}
