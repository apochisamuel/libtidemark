#ifndef TMK_WIRE_PARSER_H
#define TMK_WIRE_PARSER_H

#include "../../include/tidemark/types.h"

/*
 * Parse a complete wire message: session header + N frames.
 * Calls the frame_handler callback for each valid frame.
 * Returns number of frames processed, or negative on error.
 */
typedef int (*tmk_frame_handler_fn)(tmk_context_t *ctx,
                                    const tmk_parsed_frame_t *frame,
                                    void *user);

int tmk_parse_message(tmk_context_t *ctx,
                      const uint8_t *buf, size_t len,
                      tmk_frame_handler_fn handler, void *user);

/*
 * Parse a single frame from the wire (no session header).
 * Used internally and by harnesses that pre-parse the header.
 */
int tmk_parse_single_frame(const uint8_t *buf, size_t len,
                           tmk_parsed_frame_t *out);

#endif
