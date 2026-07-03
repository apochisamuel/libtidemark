/*
 * libtidemark — variable-length integer encoding
 *
 * LEB128 (Little Endian Base 128) encoding for compact integer
 * representation.  Used internally for timestamp compression
 * and key serialization within segment data.
 *
 * Signed integers use ZigZag encoding: positive values map to
 * even numbers, negative to odd: 0→0, -1→1, 1→2, -2→3, etc.
 * This ensures small-magnitude negative numbers use few bytes.
 *
 * Delta encoding exploits temporal locality in sorted timestamp
 * sequences — adjacent timestamps typically differ by small amounts,
 * so the deltas compress well with varint encoding.
 */
#include "varint.h"

int tmk_varint_encode_u32(uint8_t *buf, size_t len, uint32_t value) {
    if (!buf) return -1;
    int bytes = 0;
    do {
        if ((size_t)bytes >= len) return -1;
        buf[bytes] = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value) buf[bytes] |= 0x80;
        bytes++;
    } while (value);
    return bytes;
}

int tmk_varint_decode_u32(const uint8_t *buf, size_t len, uint32_t *value) {
    if (!buf || !value) return -1;
    *value = 0;
    int shift = 0;
    int bytes = 0;
    do {
        if ((size_t)bytes >= len) return -1;
        if (shift >= 35) return -1; /* overflow */
        *value |= ((uint32_t)(buf[bytes] & 0x7F)) << shift;
        shift += 7;
    } while (buf[bytes++] & 0x80);
    return bytes;
}

int tmk_varint_encode_u64(uint8_t *buf, size_t len, uint64_t value) {
    if (!buf) return -1;
    int bytes = 0;
    do {
        if ((size_t)bytes >= len) return -1;
        buf[bytes] = (uint8_t)(value & 0x7F);
        value >>= 7;
        if (value) buf[bytes] |= 0x80;
        bytes++;
    } while (value);
    return bytes;
}

int tmk_varint_decode_u64(const uint8_t *buf, size_t len, uint64_t *value) {
    if (!buf || !value) return -1;
    *value = 0;
    int shift = 0;
    int bytes = 0;
    do {
        if ((size_t)bytes >= len) return -1;
        if (shift >= 70) return -1;
        *value |= ((uint64_t)(buf[bytes] & 0x7F)) << shift;
        shift += 7;
    } while (buf[bytes++] & 0x80);
    return bytes;
}

/* ZigZag encoding for signed integers */
static inline uint32_t zigzag_encode32(int32_t v) {
    return ((uint32_t)v << 1) ^ (uint32_t)(v >> 31);
}
static inline int32_t zigzag_decode32(uint32_t v) {
    return (int32_t)((v >> 1) ^ -(v & 1));
}
static inline uint64_t zigzag_encode64(int64_t v) {
    return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63);
}
static inline int64_t zigzag_decode64(uint64_t v) {
    return (int64_t)((v >> 1) ^ -(v & 1));
}

int tmk_varint_encode_i32(uint8_t *buf, size_t len, int32_t value) {
    return tmk_varint_encode_u32(buf, len, zigzag_encode32(value));
}

int tmk_varint_decode_i32(const uint8_t *buf, size_t len, int32_t *value) {
    uint32_t raw;
    int bytes = tmk_varint_decode_u32(buf, len, &raw);
    if (bytes < 0) return bytes;
    *value = zigzag_decode32(raw);
    return bytes;
}

int tmk_varint_encode_i64(uint8_t *buf, size_t len, int64_t value) {
    return tmk_varint_encode_u64(buf, len, zigzag_encode64(value));
}

int tmk_varint_decode_i64(const uint8_t *buf, size_t len, int64_t *value) {
    uint64_t raw;
    int bytes = tmk_varint_decode_u64(buf, len, &raw);
    if (bytes < 0) return bytes;
    *value = zigzag_decode64(raw);
    return bytes;
}

size_t tmk_varint_size_u32(uint32_t value) {
    size_t size = 1;
    while (value > 0x7F) { value >>= 7; size++; }
    return size;
}

size_t tmk_varint_size_u64(uint64_t value) {
    size_t size = 1;
    while (value > 0x7F) { value >>= 7; size++; }
    return size;
}

int tmk_delta_encode(const uint64_t *values, size_t count,
                     uint8_t *buf, size_t buf_len) {
    if (!values || !buf || count == 0) return -1;

    size_t offset = 0;
    uint64_t prev = 0;

    for (size_t i = 0; i < count; i++) {
        uint64_t delta = values[i] - prev;
        int bytes = tmk_varint_encode_u64(buf + offset, buf_len - offset, delta);
        if (bytes < 0) return -1;
        offset += (size_t)bytes;
        prev = values[i];
    }

    return (int)offset;
}

int tmk_delta_decode(const uint8_t *buf, size_t buf_len,
                     uint64_t *values, size_t max_count, size_t *decoded) {
    if (!buf || !values || !decoded) return -1;

    size_t offset = 0;
    uint64_t prev = 0;
    *decoded = 0;

    while (offset < buf_len && *decoded < max_count) {
        uint64_t delta;
        int bytes = tmk_varint_decode_u64(buf + offset, buf_len - offset, &delta);
        if (bytes < 0) break;
        prev += delta;
        values[*decoded] = prev;
        (*decoded)++;
        offset += (size_t)bytes;
    }

    return (int)offset;
}
