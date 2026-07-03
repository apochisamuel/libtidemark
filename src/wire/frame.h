#ifndef TMK_WIRE_FRAME_H
#define TMK_WIRE_FRAME_H

#include "../../include/tidemark/types.h"

/*
 * Parse a frame header from the wire.
 * Returns bytes consumed (TMK_FRAME_HDR_SIZE) on success, negative on error.
 */
int tmk_parse_frame_header(const uint8_t *buf, size_t len,
                           tmk_frame_header_t *out);

/*
 * Serialize a frame header.
 * Returns TMK_FRAME_HDR_SIZE on success.
 */
int tmk_write_frame_header(uint8_t *buf, size_t len,
                           const tmk_frame_header_t *hdr,
                           const uint8_t *payload, size_t payload_len);

/*
 * Validate that a frame type is recognized.
 */
bool tmk_frame_type_valid(uint16_t ft);

#endif
