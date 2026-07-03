#ifndef TMK_SEGMENT_COMPRESS_H
#define TMK_SEGMENT_COMPRESS_H

#include <stdint.h>
#include <stddef.h>

/*
 * Simple run-length encoding for segment data compression.
 * Used during compaction to reduce segment sizes before merging.
 *
 * Format: [count:1][value:1] pairs for runs of 3+,
 *         [0xFF][literal_count:1][literal_data:N] for non-runs.
 */

int tmk_rle_encode(const uint8_t *in, size_t in_len,
                   uint8_t *out, size_t out_len);
int tmk_rle_decode(const uint8_t *in, size_t in_len,
                   uint8_t *out, size_t out_len);
size_t tmk_rle_worst_case(size_t in_len);

/*
 * Simple XOR delta encoding for time-series data where
 * consecutive values are similar.
 */
int tmk_xor_delta_encode(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_len);
int tmk_xor_delta_decode(const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_len);

#endif
