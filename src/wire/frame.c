/*
 * libtidemark — frame header wire parsing
 *
 * Frame layout (32 bytes):
 *   [0..3]   length          (uint32 LE, total including header)
 *   [4..5]   frame_type      (uint16 LE)
 *   [6..9]   sequence        (uint32 LE)
 *   [10..13] session_id      (uint32 LE)
 *   [14..15] channel_id      (uint16 LE)
 *   [16..17] partition_id    (uint16 LE)
 *   [18..19] route_id        (uint16 LE)
 *   [20..23] owner_token     (uint32 LE)
 *   [24..25] flags           (uint16 LE)
 *   [26..27] subrecord_count (uint16 LE)
 *   [28..31] payload_crc     (uint32 LE, CRC of payload bytes)
 */
#include "frame.h"
#include "crc.h"
#include "../../include/tidemark/error.h"
#include <string.h>

static inline uint16_t rd16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static inline uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
}
static inline void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v); p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

bool tmk_frame_type_valid(uint16_t ft) {
    switch (ft) {
    case TMK_FRAME_SESSION_OPEN:
    case TMK_FRAME_SESSION_CLOSE:
    case TMK_FRAME_CHANNEL_REGISTER:
    case TMK_FRAME_CHANNEL_BIND:
    case TMK_FRAME_CHANNEL_CACHE_LEASE:
    case TMK_FRAME_PARTITION_CREATE:
    case TMK_FRAME_PARTITION_BIND:
    case TMK_FRAME_PARTITION_CACHE_EPOCH:
    case TMK_FRAME_SEGMENT_CREATE:
    case TMK_FRAME_SEGMENT_RETIRE:
    case TMK_FRAME_SEGMENT_META_INIT:
    case TMK_FRAME_DATA_INGEST:
    case TMK_FRAME_DATA_INGEST_IXWR:
    case TMK_FRAME_INDEX_REGISTER:
    case TMK_FRAME_INDEX_BUILD_CROSS:
    case TMK_FRAME_INDEX_BIND_CURSOR:
    case TMK_FRAME_INDEX_REG_CAPACITY:
    case TMK_FRAME_CURSOR_OPEN:
    case TMK_FRAME_CURSOR_CLOSE:
    case TMK_FRAME_CURSOR_CHECK:
    case TMK_FRAME_CURSOR_READ_BATCH:
    case TMK_FRAME_CURSOR_SEEK:
    case TMK_FRAME_CURSOR_ATTACH_IDX:
    case TMK_FRAME_CURSOR_BIND_LEASE:
    case TMK_FRAME_LEASE_ACQUIRE:
    case TMK_FRAME_LEASE_RELEASE:
    case TMK_FRAME_LEASE_BIND:
    case TMK_FRAME_LEASE_BIND_META:
    case TMK_FRAME_EPOCH_ADVANCE:
    case TMK_FRAME_RETENTION_TRIGGER:
    case TMK_FRAME_RETENTION_CONFIG:
    case TMK_FRAME_RETENTION_REINDEX:
    case TMK_FRAME_COMPACT_SCHEDULE:
    case TMK_FRAME_COMPACT_TRIGGER:
    case TMK_FRAME_COMPACT_MERGE:
    case TMK_FRAME_PARTITION_VALIDATE:
    case TMK_FRAME_COMPACT_MERGE_PART:
    case TMK_FRAME_COMPACT_DEFER_DRAIN:
    case TMK_FRAME_CHECKPOINT_CREATE:
    case TMK_FRAME_CHECKPOINT_RESTORE:
    case TMK_FRAME_MIGRATE_BEGIN:
    case TMK_FRAME_MIGRATE_TRANSFER:
    case TMK_FRAME_MIGRATE_COMMIT:
    case TMK_FRAME_MIGRATE_ENUMERATE:
    case TMK_FRAME_MATERIALIZE_EMIT:
    case TMK_FRAME_MATERIALIZE_SCAN:
    case TMK_FRAME_MATERIALIZE_RANGE:
    case TMK_FRAME_MATERIALIZE_RESOLVE:
    case TMK_FRAME_MATERIALIZE_READ:
    case TMK_FRAME_MATERIALIZE_AGG_IDX:
    case TMK_FRAME_MATERIALIZE_ISCAN:
        return true;
    default:
        return false;
    }
}

int tmk_parse_frame_header(const uint8_t *buf, size_t len,
                           tmk_frame_header_t *out) {
    if (!buf || !out) return TMK_ERR_SHORT_BUF;
    if (len < TMK_FRAME_HDR_SIZE) return TMK_ERR_SHORT_BUF;

    out->length          = rd32(buf + 0);
    out->frame_type      = rd16(buf + 4);
    out->sequence        = rd32(buf + 6);
    out->session_id      = rd32(buf + 10);
    out->channel_id      = rd16(buf + 14);
    out->partition_id    = rd16(buf + 16);
    out->route_id        = rd16(buf + 18);
    out->owner_token     = rd32(buf + 20);
    out->flags           = rd16(buf + 24);
    out->subrecord_count = rd16(buf + 26);
    out->payload_crc     = rd32(buf + 28);

    if (out->length < TMK_FRAME_HDR_SIZE) return TMK_ERR_BAD_FRAME_LEN;
    if (out->length > TMK_MAX_FRAME_LEN) return TMK_ERR_BAD_FRAME_LEN;
    if (len < out->length) return TMK_ERR_SHORT_BUF;
    if (!tmk_frame_type_valid(out->frame_type)) return TMK_ERR_BAD_FRAME_TYPE;
    if (out->subrecord_count > TMK_MAX_SUBRECORDS) return TMK_ERR_BAD_SUBREC_LEN;

    /* Validate payload CRC */
    uint32_t payload_len = out->length - TMK_FRAME_HDR_SIZE;
    if (payload_len > 0) {
        uint32_t computed = tmk_crc32c(buf + TMK_FRAME_HDR_SIZE, payload_len);
        if (computed != out->payload_crc) return TMK_ERR_BAD_CRC;
    }

    return TMK_FRAME_HDR_SIZE;
}

int tmk_write_frame_header(uint8_t *buf, size_t len,
                           const tmk_frame_header_t *hdr,
                           const uint8_t *payload, size_t payload_len) {
    size_t total = TMK_FRAME_HDR_SIZE + payload_len;
    if (!buf || !hdr || len < total) return TMK_ERR_SHORT_BUF;

    memset(buf, 0, TMK_FRAME_HDR_SIZE);
    wr32(buf + 0, (uint32_t)total);
    wr16(buf + 4, hdr->frame_type);
    wr32(buf + 6, hdr->sequence);
    wr32(buf + 10, hdr->session_id);
    wr16(buf + 14, hdr->channel_id);
    wr16(buf + 16, hdr->partition_id);
    wr16(buf + 18, hdr->route_id);
    wr32(buf + 20, hdr->owner_token);
    wr16(buf + 24, hdr->flags);
    wr16(buf + 26, hdr->subrecord_count);

    if (payload && payload_len > 0) {
        memcpy(buf + TMK_FRAME_HDR_SIZE, payload, payload_len);
        wr32(buf + 28, tmk_crc32c(payload, payload_len));
    } else {
        wr32(buf + 28, tmk_crc32c(NULL, 0));
    }

    return (int)total;
}
