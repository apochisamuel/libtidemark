#ifndef TMK_CHECKPOINT_INCREMENTAL_H
#define TMK_CHECKPOINT_INCREMENTAL_H

#include "../../include/tidemark/types.h"

/*
 * Incremental checkpointing — captures only segments modified
 * since the last checkpoint, reducing checkpoint overhead.
 */

typedef struct {
    uint16_t checkpoint_id;
    uint16_t base_checkpoint_id;
    uint16_t modified_segment_ids[TMK_MAX_SEGMENTS];
    uint16_t modified_count;
    uint32_t base_epoch;
    uint32_t current_epoch;
    uint32_t bytes_saved;
} tmk_incremental_checkpoint_t;

int tmk_checkpoint_incremental(tmk_context_t *ctx,
                               uint16_t base_checkpoint_id,
                               tmk_incremental_checkpoint_t *out);
int tmk_checkpoint_find_modified(tmk_context_t *ctx,
                                 uint32_t since_epoch,
                                 uint16_t *out_ids,
                                 uint16_t max_count,
                                 uint16_t *out_count);
int tmk_checkpoint_compute_delta_size(tmk_context_t *ctx,
                                     const tmk_incremental_checkpoint_t *incr,
                                     size_t *out_size);
bool tmk_checkpoint_is_incremental_worthwhile(tmk_context_t *ctx,
                                              uint16_t base_checkpoint_id);

#endif
