/*
 * libtidemark — protocol version negotiation
 *
 * Capability-based negotiation where both sides advertise their
 * supported features and limits.  The agreed-upon set is the
 * intersection of capabilities, the minimum of version ranges,
 * and the minimum of resource limits.
 *
 * Serialization format (28 bytes):
 *   [0..1]   version_min
 *   [2..3]   version_max
 *   [4..5]   capabilities
 *   [6..7]   padding
 *   [8..11]  max_frame_size
 *   [12..15] max_segment_capacity
 *   [16..17] max_sessions
 *   [18..19] max_channels
 *   [20..21] max_partitions
 *   [22..23] padding
 *   [24..27] CRC-32C of bytes 0..23
 */
#include "negotiate.h"
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

static uint16_t min16(uint16_t a, uint16_t b) { return a < b ? a : b; }
static uint16_t max16(uint16_t a, uint16_t b) { return a > b ? a : b; }
static uint32_t min32(uint32_t a, uint32_t b) { return a < b ? a : b; }

int tmk_negotiate_init_offer(tmk_negotiate_offer_t *offer) {
    if (!offer) return TMK_ERR_SHORT_BUF;
    offer->version_min = TMK_VERSION;
    offer->version_max = TMK_VERSION;
    offer->capabilities = TMK_CAP_ALL;
    offer->max_frame_size = TMK_MAX_FRAME_LEN;
    offer->max_segment_capacity = TMK_MAX_SEGMENT_CAP;
    offer->max_sessions = TMK_MAX_SESSIONS;
    offer->max_channels = TMK_MAX_CHANNELS;
    offer->max_partitions = TMK_MAX_PARTITIONS;
    return TMK_OK;
}

int tmk_negotiate_process(const tmk_negotiate_offer_t *local,
                          const tmk_negotiate_offer_t *remote,
                          tmk_negotiate_result_t *result) {
    if (!local || !remote || !result) return TMK_ERR_SHORT_BUF;
    memset(result, 0, sizeof(*result));

    /* Check version overlap */
    uint16_t overlap_min = max16(local->version_min, remote->version_min);
    uint16_t overlap_max = min16(local->version_max, remote->version_max);

    if (overlap_min > overlap_max) {
        result->compatible = false;
        result->negotiation_complete = true;
        return TMK_ERR_BAD_VERSION;
    }

    result->agreed_version = overlap_max; /* use highest compatible */
    result->agreed_capabilities = local->capabilities & remote->capabilities;
    result->agreed_max_frame = min32(local->max_frame_size, remote->max_frame_size);
    result->agreed_max_segment = min32(local->max_segment_capacity,
                                       remote->max_segment_capacity);

    /* Must have at least BASIC capability */
    if (!(result->agreed_capabilities & TMK_CAP_BASIC)) {
        result->compatible = false;
        result->negotiation_complete = true;
        return TMK_ERR_BAD_STATE;
    }

    result->compatible = true;
    result->negotiation_complete = true;
    return TMK_OK;
}

int tmk_negotiate_serialize(const tmk_negotiate_offer_t *offer,
                            uint8_t *buf, size_t len) {
    if (!offer || !buf || len < 28) return TMK_ERR_SHORT_BUF;

    memset(buf, 0, 28);
    wr16(buf + 0, offer->version_min);
    wr16(buf + 2, offer->version_max);
    wr16(buf + 4, offer->capabilities);
    wr32(buf + 8, offer->max_frame_size);
    wr32(buf + 12, offer->max_segment_capacity);
    wr16(buf + 16, offer->max_sessions);
    wr16(buf + 18, offer->max_channels);
    wr16(buf + 20, offer->max_partitions);
    wr32(buf + 24, tmk_crc32c(buf, 24));

    return 28;
}

int tmk_negotiate_deserialize(const uint8_t *buf, size_t len,
                              tmk_negotiate_offer_t *offer) {
    if (!buf || !offer || len < 28) return TMK_ERR_SHORT_BUF;

    uint32_t stored_crc = rd32(buf + 24);
    uint32_t computed_crc = tmk_crc32c(buf, 24);
    if (stored_crc != computed_crc) return TMK_ERR_BAD_CRC;

    offer->version_min = rd16(buf + 0);
    offer->version_max = rd16(buf + 2);
    offer->capabilities = rd16(buf + 4);
    offer->max_frame_size = rd32(buf + 8);
    offer->max_segment_capacity = rd32(buf + 12);
    offer->max_sessions = rd16(buf + 16);
    offer->max_channels = rd16(buf + 18);
    offer->max_partitions = rd16(buf + 20);

    return 28;
}

bool tmk_negotiate_has_capability(const tmk_negotiate_result_t *result,
                                  uint16_t cap) {
    if (!result || !result->negotiation_complete) return false;
    return (result->agreed_capabilities & cap) != 0;
}
