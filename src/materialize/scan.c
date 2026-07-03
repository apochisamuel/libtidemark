/*
 * libtidemark — partition scan materialization
 *
 * Scans all segments in a partition for query results.
 * Uses the channel's cached segment list for fast access.
 *
 * BUG SURFACE (migrated_partition_ref): Channel cache may be stale.
 */
#include "../internal/context.h"
#include "scan.h"
#include "../channel/router.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_materialize_scan_partition(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    uint16_t part_id = frame->header.partition_id;
    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    int scanned = 0;
    for (uint16_t i = 0; i < part->segment_count; i++) {
        tmk_segment_t *seg = tmk_find_segment(ctx, part->segment_ids[i]);
        if (seg && seg->occupied && seg->data && seg->used > 0) {
            volatile uint8_t check = seg->data[0];
            (void)check;
            scanned++;
        }
    }

    return scanned;
}
