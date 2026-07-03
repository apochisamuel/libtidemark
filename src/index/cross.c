/*
 * libtidemark — cross-partition index building
 *
 * Builds an index node that spans multiple partitions by storing
 * references to partition metadata.
 *
 * BUG SURFACE (partition_merge_cursor): Partition merge frees
 * the source partition but cross-partition index entries still
 * reference it.
 */
#include "../internal/context.h"
#include "cross.h"
#include "../session/manager.h"
#include "../../include/tidemark/error.h"

int tmk_index_build_cross_partition(tmk_context_t *ctx, const tmk_parsed_frame_t *frame) {
    int sv = tmk_session_validate(ctx, frame);
    if (sv < 0) return sv;

    if (frame->parsed_sub_count < 1) return TMK_ERR_BAD_STATE;
    if (frame->subrecords[0].kind != TMK_SUB_INDEX_ENTRY)
        return TMK_ERR_BAD_SUBREC_KIND;

    uint16_t idx_id = frame->header.route_id;
    uint16_t part_id = frame->header.partition_id;

    tmk_index_node_t *idx = tmk_find_index_node(ctx, idx_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;

    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    idx->partition_ref = part;
    idx->partition_id = part_id;

    return TMK_OK;
}
