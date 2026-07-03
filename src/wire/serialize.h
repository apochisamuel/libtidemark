#ifndef TMK_WIRE_SERIALIZE_H
#define TMK_WIRE_SERIALIZE_H

#include "../../include/tidemark/types.h"

/*
 * High-level message serialization — constructs a complete wire
 * message (session header + frames + subrecords) in a single buffer.
 *
 * Used by the PoC generator and test harnesses.
 */

typedef struct {
    uint8_t *buf;
    size_t   capacity;
    size_t   offset;
    uint32_t session_id;
    uint32_t epoch;
    uint32_t sequence;
    uint16_t frame_count;
    size_t   header_offset;
} tmk_serializer_t;

int tmk_serializer_init(tmk_serializer_t *ser, uint8_t *buf, size_t capacity,
                        uint32_t session_id, uint32_t epoch);
int tmk_serializer_begin_frame(tmk_serializer_t *ser,
                               uint16_t frame_type,
                               uint16_t channel_id,
                               uint16_t partition_id,
                               uint16_t route_id,
                               uint32_t owner_token,
                               uint16_t flags);
int tmk_serializer_add_subrecord(tmk_serializer_t *ser,
                                 uint16_t kind,
                                 uint32_t local_epoch,
                                 uint32_t generation,
                                 uint32_t lease_token,
                                 const uint8_t *payload,
                                 uint16_t payload_len);
int tmk_serializer_end_frame(tmk_serializer_t *ser);
int tmk_serializer_finalize(tmk_serializer_t *ser);
size_t tmk_serializer_size(const tmk_serializer_t *ser);

#endif
