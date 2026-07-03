#ifndef TMK_CHANNEL_VALIDATE_H
#define TMK_CHANNEL_VALIDATE_H

#include "../../include/tidemark/types.h"

/*
 * Channel epoch validation — verifies that a frame's epoch matches
 * the channel's bound partition's cached epoch state.
 *
 * This is a fast-path check that avoids expensive partition lookups
 * by using the channel's cached epoch pointer.
 *
 * BUG SURFACE (epoch_session_stale): If the session's epoch state
 * pointer (cached via channel) has been freed by epoch cascade,
 * this validation dereferences freed memory.
 */

int tmk_channel_validate_epoch(tmk_context_t *ctx, uint16_t channel_id,
                               const tmk_parsed_frame_t *frame);

#endif
