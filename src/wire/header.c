/*
 * libtidemark — session header wire parsing
 *
 * Session header layout (24 bytes):
 *   [0..3]   magic      (uint32 LE)
 *   [4..5]   version    (uint16 LE)
 *   [6..9]   session_id (uint32 LE)
 *   [10..13] epoch      (uint32 LE)
 *   [14..15] frame_count(uint16 LE)
 *   [16..17] reserved   (uint16 LE)
 *   [18..19] padding    (zero)
 *   [20..23] header_crc (uint32 LE, CRC-32C of bytes 0..19)
 */
#include "header.h"
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

int tmk_parse_session_header(const uint8_t *buf, size_t len,
                             tmk_session_header_t *out) {
    if (!buf || !out) return TMK_ERR_SHORT_BUF;
    if (len < TMK_SESSION_HDR_SIZE) return TMK_ERR_SHORT_BUF;

    out->magic       = rd32(buf + 0);
    out->version     = rd16(buf + 4);
    out->session_id  = rd32(buf + 6);
    out->epoch       = rd32(buf + 10);
    out->frame_count = rd16(buf + 14);
    out->reserved    = rd16(buf + 16);
    out->header_crc  = rd32(buf + 20);

    if (out->magic != TMK_MAGIC) return TMK_ERR_BAD_MAGIC;
    if (out->version != TMK_VERSION) return TMK_ERR_BAD_VERSION;

    uint32_t computed = tmk_crc32c(buf, 20);
    if (computed != out->header_crc) return TMK_ERR_BAD_CRC;

    if (out->frame_count == 0) return TMK_ERR_BAD_FRAME_LEN;

    return TMK_SESSION_HDR_SIZE;
}

int tmk_write_session_header(uint8_t *buf, size_t len,
                             const tmk_session_header_t *hdr) {
    if (!buf || !hdr) return TMK_ERR_SHORT_BUF;
    if (len < TMK_SESSION_HDR_SIZE) return TMK_ERR_SHORT_BUF;

    memset(buf, 0, TMK_SESSION_HDR_SIZE);
    wr32(buf + 0, hdr->magic);
    wr16(buf + 4, hdr->version);
    wr32(buf + 6, hdr->session_id);
    wr32(buf + 10, hdr->epoch);
    wr16(buf + 14, hdr->frame_count);
    wr16(buf + 16, hdr->reserved);
    wr32(buf + 20, tmk_crc32c(buf, 20));

    return TMK_SESSION_HDR_SIZE;
}
