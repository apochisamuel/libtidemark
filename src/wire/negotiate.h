#ifndef TMK_WIRE_NEGOTIATE_H
#define TMK_WIRE_NEGOTIATE_H

#include "../../include/tidemark/types.h"

/*
 * Protocol version negotiation.
 *
 * Implements a capability-based negotiation handshake where
 * client and server exchange supported feature flags and
 * agree on a protocol version and feature set.
 */

#define TMK_CAP_BASIC           0x0001
#define TMK_CAP_COMPRESSION     0x0002
#define TMK_CAP_ENCRYPTION      0x0004
#define TMK_CAP_DELTA_ENCODING  0x0008
#define TMK_CAP_CHECKPOINT      0x0010
#define TMK_CAP_MIGRATION       0x0020
#define TMK_CAP_INCREMENTAL_CP  0x0040
#define TMK_CAP_AGGREGATE       0x0080
#define TMK_CAP_BTREE_INDEX     0x0100
#define TMK_CAP_SORTED_MERGE    0x0200
#define TMK_CAP_ALL             0x03FF

typedef struct {
    uint16_t version_min;
    uint16_t version_max;
    uint16_t capabilities;
    uint32_t max_frame_size;
    uint32_t max_segment_capacity;
    uint16_t max_sessions;
    uint16_t max_channels;
    uint16_t max_partitions;
} tmk_negotiate_offer_t;

typedef struct {
    uint16_t agreed_version;
    uint16_t agreed_capabilities;
    uint32_t agreed_max_frame;
    uint32_t agreed_max_segment;
    bool     negotiation_complete;
    bool     compatible;
} tmk_negotiate_result_t;

int tmk_negotiate_init_offer(tmk_negotiate_offer_t *offer);
int tmk_negotiate_process(const tmk_negotiate_offer_t *local,
                          const tmk_negotiate_offer_t *remote,
                          tmk_negotiate_result_t *result);
int tmk_negotiate_serialize(const tmk_negotiate_offer_t *offer,
                            uint8_t *buf, size_t len);
int tmk_negotiate_deserialize(const uint8_t *buf, size_t len,
                              tmk_negotiate_offer_t *offer);
bool tmk_negotiate_has_capability(const tmk_negotiate_result_t *result,
                                  uint16_t cap);

#endif
