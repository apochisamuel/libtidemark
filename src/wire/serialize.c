/*
 * libtidemark — message serialization
 *
 * Builds complete wire messages incrementally:
 *   1. init() — reserves space for session header
 *   2. begin_frame() — writes frame header (placeholder CRC)
 *   3. add_subrecord() — appends subrecords to current frame
 *   4. end_frame() — backpatches frame length, count, and CRC
 *   5. finalize() — backpatches session header with frame count and CRC
 *
 * This ensures CRC integrity at both the session and frame levels.
 */
#include "serialize.h"
#include "crc.h"
#include "header.h"
#include "frame.h"
#include "subrecord.h"
#include "../../include/tidemark/error.h"
#include <string.h>

static inline void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
}
static inline void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int tmk_serializer_init(tmk_serializer_t *ser, uint8_t *buf, size_t capacity,
                        uint32_t session_id, uint32_t epoch) {
    if (!ser || !buf || capacity < TMK_SESSION_HDR_SIZE + TMK_FRAME_HDR_SIZE)
        return TMK_ERR_SHORT_BUF;

    memset(ser, 0, sizeof(*ser));
    ser->buf = buf;
    ser->capacity = capacity;
    ser->session_id = session_id;
    ser->epoch = epoch;
    ser->sequence = 0;
    ser->frame_count = 0;
    ser->header_offset = 0;

    /* Reserve space for session header (will be filled in finalize) */
    memset(buf, 0, TMK_SESSION_HDR_SIZE);
    ser->offset = TMK_SESSION_HDR_SIZE;

    return TMK_OK;
}

/* Track current frame's position and subrecord count */
static size_t cur_frame_start = 0;
static uint16_t cur_sub_count = 0;

int tmk_serializer_begin_frame(tmk_serializer_t *ser,
                               uint16_t frame_type,
                               uint16_t channel_id,
                               uint16_t partition_id,
                               uint16_t route_id,
                               uint32_t owner_token,
                               uint16_t flags) {
    if (!ser) return TMK_ERR_SHORT_BUF;
    if (ser->offset + TMK_FRAME_HDR_SIZE > ser->capacity) return TMK_ERR_SHORT_BUF;

    cur_frame_start = ser->offset;
    cur_sub_count = 0;

    uint8_t *p = ser->buf + ser->offset;
    memset(p, 0, TMK_FRAME_HDR_SIZE);

    /* Write frame header fields (length and CRC will be patched in end_frame) */
    wr32(p + 0, 0);  /* length placeholder */
    wr16(p + 4, frame_type);
    wr32(p + 6, ++ser->sequence);
    wr32(p + 10, ser->session_id);
    wr16(p + 14, channel_id);
    wr16(p + 16, partition_id);
    wr16(p + 18, route_id);
    wr32(p + 20, owner_token);
    wr16(p + 24, flags);
    wr16(p + 26, 0);  /* subrecord count placeholder */
    wr32(p + 28, 0);  /* payload CRC placeholder */

    ser->offset += TMK_FRAME_HDR_SIZE;

    return TMK_OK;
}

int tmk_serializer_add_subrecord(tmk_serializer_t *ser,
                                 uint16_t kind,
                                 uint32_t local_epoch,
                                 uint32_t generation,
                                 uint32_t lease_token,
                                 const uint8_t *payload,
                                 uint16_t payload_len) {
    if (!ser) return TMK_ERR_SHORT_BUF;
    size_t needed = TMK_SUBREC_HDR_SIZE + payload_len;
    if (ser->offset + needed > ser->capacity) return TMK_ERR_SHORT_BUF;

    uint8_t *p = ser->buf + ser->offset;
    wr16(p + 0, kind);
    wr16(p + 2, payload_len);
    wr32(p + 4, local_epoch);
    wr32(p + 8, generation);
    wr32(p + 12, lease_token);

    if (payload && payload_len > 0) {
        memcpy(p + TMK_SUBREC_HDR_SIZE, payload, payload_len);
    }

    ser->offset += needed;
    cur_sub_count++;

    return TMK_OK;
}

int tmk_serializer_end_frame(tmk_serializer_t *ser) {
    if (!ser) return TMK_ERR_SHORT_BUF;

    uint8_t *frame_hdr = ser->buf + cur_frame_start;
    uint32_t frame_len = (uint32_t)(ser->offset - cur_frame_start);

    /* Backpatch length */
    wr32(frame_hdr + 0, frame_len);

    /* Backpatch subrecord count */
    wr16(frame_hdr + 26, cur_sub_count);

    /* Backpatch payload CRC */
    uint32_t payload_len = frame_len - TMK_FRAME_HDR_SIZE;
    if (payload_len > 0) {
        uint32_t crc = tmk_crc32c(frame_hdr + TMK_FRAME_HDR_SIZE, payload_len);
        wr32(frame_hdr + 28, crc);
    }

    ser->frame_count++;
    return TMK_OK;
}

int tmk_serializer_finalize(tmk_serializer_t *ser) {
    if (!ser) return TMK_ERR_SHORT_BUF;

    uint8_t *hdr = ser->buf;

    /* Write session header */
    wr32(hdr + 0, TMK_MAGIC);
    wr16(hdr + 4, TMK_VERSION);
    wr32(hdr + 6, ser->session_id);
    wr32(hdr + 10, ser->epoch);
    wr16(hdr + 14, ser->frame_count);
    wr16(hdr + 16, 0); /* reserved */
    /* padding already zero */
    wr32(hdr + 20, tmk_crc32c(hdr, 20));

    return TMK_OK;
}

size_t tmk_serializer_size(const tmk_serializer_t *ser) {
    return ser ? ser->offset : 0;
}
