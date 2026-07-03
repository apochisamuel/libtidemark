#ifndef TMK_WIRE_SUBRECORD_H
#define TMK_WIRE_SUBRECORD_H

#include "../../include/tidemark/types.h"

/*
 * Parse subrecords from frame payload.
 * Returns total bytes consumed on success, negative on error.
 */
int tmk_parse_subrecords(const uint8_t *payload, size_t payload_len,
                         uint16_t expected_count,
                         tmk_subrecord_t *out, uint16_t *parsed_count);

/*
 * Serialize a single subrecord.
 * Returns bytes written on success, negative on error.
 */
int tmk_write_subrecord(uint8_t *buf, size_t len,
                        const tmk_subrecord_t *sub,
                        const uint8_t *payload, size_t payload_len);

bool tmk_subrecord_kind_valid(uint16_t kind);

#endif
