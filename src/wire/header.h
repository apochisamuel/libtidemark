#ifndef TMK_WIRE_HEADER_H
#define TMK_WIRE_HEADER_H

#include "../../include/tidemark/types.h"

/*
 * Parse a session header from the wire.
 * Returns bytes consumed (TMK_SESSION_HDR_SIZE) on success, negative on error.
 */
int tmk_parse_session_header(const uint8_t *buf, size_t len,
                             tmk_session_header_t *out);

/*
 * Serialize a session header to wire format.
 * buf must be at least TMK_SESSION_HDR_SIZE bytes.
 * Returns TMK_SESSION_HDR_SIZE on success.
 */
int tmk_write_session_header(uint8_t *buf, size_t len,
                             const tmk_session_header_t *hdr);

#endif
