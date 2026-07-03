#ifndef TMK_INTERNAL_VARINT_H
#define TMK_INTERNAL_VARINT_H

#include <stdint.h>
#include <stddef.h>

/*
 * Variable-length integer encoding/decoding for compact timestamp
 * and key serialization within segment data buffers.
 *
 * Uses LEB128 encoding: each byte stores 7 bits of data plus a
 * continuation bit.  Values 0-127 use 1 byte; larger values use
 * progressively more bytes up to 10 (for 64-bit values).
 */

int tmk_varint_encode_u32(uint8_t *buf, size_t len, uint32_t value);
int tmk_varint_decode_u32(const uint8_t *buf, size_t len, uint32_t *value);

int tmk_varint_encode_u64(uint8_t *buf, size_t len, uint64_t value);
int tmk_varint_decode_u64(const uint8_t *buf, size_t len, uint64_t *value);

int tmk_varint_encode_i32(uint8_t *buf, size_t len, int32_t value);
int tmk_varint_decode_i32(const uint8_t *buf, size_t len, int32_t *value);

int tmk_varint_encode_i64(uint8_t *buf, size_t len, int64_t value);
int tmk_varint_decode_i64(const uint8_t *buf, size_t len, int64_t *value);

size_t tmk_varint_size_u32(uint32_t value);
size_t tmk_varint_size_u64(uint64_t value);

/* Delta encoding for sorted timestamp sequences */
int tmk_delta_encode(const uint64_t *values, size_t count,
                     uint8_t *buf, size_t buf_len);
int tmk_delta_decode(const uint8_t *buf, size_t buf_len,
                     uint64_t *values, size_t max_count, size_t *decoded);

#endif
