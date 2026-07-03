/*
 * libtidemark — partition observer
 *
 * Monitors partition state changes and provides query utilities
 * for partition management.  Tracks ownership transfers, generation
 * changes, and segment additions/removals.
 *
 * The observer pattern is used by migration and compaction subsystems
 * to react to partition state changes without tight coupling.
 */
#include "../internal/context.h"
#include "observer.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_partition_add_segment(tmk_context_t *ctx, uint16_t partition_id,
                              uint16_t segment_id) {
    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    /* Check for duplicate */
    for (uint16_t i = 0; i < part->segment_count; i++) {
        if (part->segment_ids[i] == segment_id)
            return TMK_ERR_ALREADY_EXISTS;
    }

    if (part->segment_count >= TMK_MAX_SEGMENTS)
        return TMK_ERR_OVERFLOW;

    part->segment_ids[part->segment_count] = segment_id;
    part->segment_count++;

    /* Update segment's partition reference */
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (seg && seg->occupied) {
        seg->partition_id = partition_id;
    }

    return TMK_OK;
}

int tmk_partition_remove_segment(tmk_context_t *ctx, uint16_t partition_id,
                                 uint16_t segment_id) {
    tmk_partition_t *part = tmk_find_partition(ctx, partition_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    for (uint16_t i = 0; i < part->segment_count; i++) {
        if (part->segment_ids[i] == segment_id) {
            /* Shift remaining elements */
            if (i + 1 < part->segment_count) {
                memmove(&part->segment_ids[i], &part->segment_ids[i + 1],
                        (part->segment_count - i - 1) * sizeof(uint16_t));
            }
            part->segment_count--;
            return TMK_OK;
        }
    }

    return TMK_ERR_NOT_FOUND;
}

int tmk_partition_find_mergeable(tmk_context_t *ctx,
                                  tmk_partition_t **part_a,
                                  tmk_partition_t **part_b) {
    if (!part_a || !part_b) return TMK_ERR_SHORT_BUF;
    *part_a = NULL;
    *part_b = NULL;

    /* Find two partitions with lowest segment counts for merging */
    int min_a = -1, min_b = -1;
    uint16_t min_a_count = UINT16_MAX, min_b_count = UINT16_MAX;

    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (!ctx->partitions[i].active) continue;
        if (ctx->partitions[i].phase != TMK_PHASE_ACTIVE) continue;

        if (ctx->partitions[i].segment_count < min_a_count) {
            min_b = min_a;
            min_b_count = min_a_count;
            min_a = i;
            min_a_count = ctx->partitions[i].segment_count;
        } else if (ctx->partitions[i].segment_count < min_b_count) {
            min_b = i;
            min_b_count = ctx->partitions[i].segment_count;
        }
    }

    if (min_a >= 0 && min_b >= 0) {
        *part_a = &ctx->partitions[min_a];
        *part_b = &ctx->partitions[min_b];
        return TMK_OK;
    }

    return TMK_ERR_NOT_FOUND;
}

int tmk_partition_count_active(tmk_context_t *ctx) {
    int count = 0;
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (ctx->partitions[i].active) count++;
    }
    return count;
}

int tmk_partition_total_segments(tmk_context_t *ctx) {
    int total = 0;
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (ctx->partitions[i].active) {
            total += ctx->partitions[i].segment_count;
        }
    }
    return total;
}

int tmk_partition_total_bytes(tmk_context_t *ctx) {
    int total = 0;
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (!ctx->partitions[i].active) continue;
        for (uint16_t j = 0; j < ctx->partitions[i].segment_count; j++) {
            tmk_segment_t *seg = tmk_find_segment(ctx,
                ctx->partitions[i].segment_ids[j]);
            if (seg && seg->occupied) {
                total += (int)seg->used;
            }
        }
    }
    return total;
}

uint16_t tmk_partition_largest(tmk_context_t *ctx) {
    uint16_t best_id = 0;
    uint16_t best_count = 0;
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (ctx->partitions[i].active &&
            ctx->partitions[i].segment_count > best_count) {
            best_count = ctx->partitions[i].segment_count;
            best_id = ctx->partitions[i].id;
        }
    }
    return best_id;
}

uint16_t tmk_partition_smallest(tmk_context_t *ctx) {
    uint16_t best_id = 0;
    uint16_t best_count = UINT16_MAX;
    for (int i = 0; i < TMK_MAX_PARTITIONS; i++) {
        if (ctx->partitions[i].active &&
            ctx->partitions[i].segment_count < best_count) {
            best_count = ctx->partitions[i].segment_count;
            best_id = ctx->partitions[i].id;
        }
    }
    return best_id;
}

bool tmk_partition_has_active_leases(tmk_context_t *ctx, uint16_t partition_id) {
    for (int i = 0; i < TMK_MAX_LEASES; i++) {
        if (!ctx->leases[i].active) continue;
        tmk_segment_t *seg = tmk_find_segment(ctx, ctx->leases[i].segment_id);
        if (seg && seg->partition_id == partition_id) return true;
    }
    return false;
}

bool tmk_partition_has_active_cursors(tmk_context_t *ctx, uint16_t partition_id) {
    for (int i = 0; i < TMK_MAX_CURSORS; i++) {
        if (ctx->cursors[i].active &&
            ctx->cursors[i].partition_id == partition_id) {
            return true;
        }
    }
    return false;
}

/*
 * Validate partition's cached channel descriptor.
 * BUG: After retention evicts the channel (freeing channel_data),
 * the partition's cached_channel_desc pointer is stale.
 */
int tmk_partition_validate_channel_desc(tmk_context_t *ctx, uint16_t part_id) {
    tmk_partition_t *part = tmk_find_partition(ctx, part_id);
    if (!part || !part->active) return TMK_ERR_NOT_FOUND;

    if (part->cached_channel_desc) {
        /* READ through potentially stale pointer → UAF after eviction */
        volatile uint8_t check = part->cached_channel_desc[0];
        (void)check;
        return TMK_OK;
    }
    return TMK_ERR_NOT_FOUND;
}
