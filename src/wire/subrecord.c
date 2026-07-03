/*
 * libtidemark — subrecord wire parsing
 *
 * Subrecord layout (16 bytes header + variable payload):
 *   [0..1]   kind        (uint16 LE)
 *   [2..3]   length      (uint16 LE, payload bytes only)
 *   [4..7]   local_epoch (uint32 LE)
 *   [8..11]  generation  (uint32 LE)
 *   [12..15] lease_token (uint32 LE)
 *   [16..]   payload     (length bytes)
 */
#include "subrecord.h"
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

bool tmk_subrecord_kind_valid(uint16_t kind) {
    switch (kind) {
    case TMK_SUB_DATA:
    case TMK_SUB_METADATA:
    case TMK_SUB_TOMBSTONE:
    case TMK_SUB_CONTROL:
    case TMK_SUB_INDEX_ENTRY:
    case TMK_SUB_LEASE_TOKEN:
    case TMK_SUB_CHECKPOINT_REF:
    case TMK_SUB_MIGRATION_DESC:
    case TMK_SUB_EPOCH_STATE:
    case TMK_SUB_RETENTION_POLICY:
    case TMK_SUB_CAPACITY_INFO:
    case TMK_SUB_CURSOR_POS:
        return true;
    default:
        return false;
    }
}

int tmk_parse_subrecords(const uint8_t *payload, size_t payload_len,
                         uint16_t expected_count,
                         tmk_subrecord_t *out, uint16_t *parsed_count) {
    if (!payload || !out || !parsed_count) return TMK_ERR_SHORT_BUF;
    *parsed_count = 0;
    size_t offset = 0;

    for (uint16_t i = 0; i < expected_count; i++) {
        if (offset + TMK_SUBREC_HDR_SIZE > payload_len)
            return TMK_ERR_SHORT_BUF;

        out[i].kind        = rd16(payload + offset);
        out[i].length      = rd16(payload + offset + 2);
        out[i].local_epoch = rd32(payload + offset + 4);
        out[i].generation  = rd32(payload + offset + 8);
        out[i].lease_token = rd32(payload + offset + 12);

        if (!tmk_subrecord_kind_valid(out[i].kind))
            return TMK_ERR_BAD_SUBREC_KIND;

        offset += TMK_SUBREC_HDR_SIZE;

        if (out[i].length > 0) {
            if (offset + out[i].length > payload_len)
                return TMK_ERR_BAD_SUBREC_LEN;
            out[i].payload = payload + offset;
            offset += out[i].length;
        } else {
            out[i].payload = NULL;
        }

        (*parsed_count)++;
    }

    return (int)offset;
}

int tmk_write_subrecord(uint8_t *buf, size_t len,
                        const tmk_subrecord_t *sub,
                        const uint8_t *payload, size_t payload_len) {
    size_t total = TMK_SUBREC_HDR_SIZE + payload_len;
    if (!buf || !sub || len < total) return TMK_ERR_SHORT_BUF;

    wr16(buf + 0, sub->kind);
    wr16(buf + 2, (uint16_t)payload_len);
    wr32(buf + 4, sub->local_epoch);
    wr32(buf + 8, sub->generation);
    wr32(buf + 12, sub->lease_token);

    if (payload && payload_len > 0) {
        memcpy(buf + TMK_SUBREC_HDR_SIZE, payload, payload_len);
    }

    return (int)total;
}
