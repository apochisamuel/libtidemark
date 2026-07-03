#ifndef TMK_WIRE_VALIDATE_H
#define TMK_WIRE_VALIDATE_H

#include "../../include/tidemark/types.h"

/*
 * Comprehensive wire-level validation beyond basic parsing.
 * Checks semantic constraints on frame sequences, subrecord
 * combinations, and cross-frame references.
 */

int tmk_validate_frame_sequence(const tmk_parsed_frame_t *frames, int count);
int tmk_validate_subrecord_combination(const tmk_parsed_frame_t *frame);
int tmk_validate_id_ranges(const tmk_parsed_frame_t *frame);
int tmk_validate_epoch_ordering(const tmk_parsed_frame_t *frames, int count);
int tmk_validate_owner_consistency(const tmk_parsed_frame_t *frames, int count);

#endif
