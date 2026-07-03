/*
 * libtidemark — source partition retirement after migration
 *
 * Retires segments from the source partition after migration.
 * Freed segments may still be referenced by index entries.
 *
 * BUG SURFACE (checkpoint_segment_dangle): Index entries that
 * reference retired segments are NOT updated.  Materialization
 * through index-resolved reads dereferences freed segment pointers.
 */
#include "../internal/context.h"
#include "retire.h"
#include "../segment/alloc.h"
#include "../../include/tidemark/error.h"

int tmk_migrate_retire_source(tmk_context_t *ctx, uint16_t source_partition_id) {
    tmk_partition_t *part = tmk_find_partition(ctx, source_partition_id);
    if (!part) return TMK_ERR_NOT_FOUND;

    /* Retire all remaining segments in the source partition */
    for (uint16_t i = 0; i < part->segment_count; i++) {
        tmk_segment_t *seg = tmk_find_segment(ctx, part->segment_ids[i]);
        if (seg && seg->occupied) {
            seg->phase = TMK_PHASE_RETIRED;
            tmk_segment_free(ctx, seg->id);
        }
    }

    /* Deactivate source partition */
    part->active = false;
    part->phase = TMK_PHASE_FREED;
    part->segment_count = 0;

    return TMK_OK;
}
