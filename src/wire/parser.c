/*
 * libtidemark — wire protocol parser
 *
 * Parses nested validated messages:
 *   session_header → frame[0] → subrecords → frame[1] → ...
 *
 * Each frame and subrecord is individually CRC-validated.
 * Invalid frames are skipped (the parser advances past them).
 */
#include "parser.h"
#include "header.h"
#include "frame.h"
#include "subrecord.h"
#include "crc.h"
#include "../../include/tidemark/error.h"

int tmk_parse_single_frame(const uint8_t *buf, size_t len,
                           tmk_parsed_frame_t *out) {
    if (!buf || !out || len < TMK_FRAME_HDR_SIZE)
        return TMK_ERR_SHORT_BUF;

    int hdr_rc = tmk_parse_frame_header(buf, len, &out->header);
    if (hdr_rc < 0) return hdr_rc;

    out->parsed_sub_count = 0;

    uint32_t payload_len = out->header.length - TMK_FRAME_HDR_SIZE;
    if (payload_len > 0 && out->header.subrecord_count > 0) {
        int sub_rc = tmk_parse_subrecords(
            buf + TMK_FRAME_HDR_SIZE, payload_len,
            out->header.subrecord_count,
            out->subrecords, &out->parsed_sub_count);
        if (sub_rc < 0) return sub_rc;
    }

    return (int)out->header.length;
}

int tmk_parse_message(tmk_context_t *ctx,
                      const uint8_t *buf, size_t len,
                      tmk_frame_handler_fn handler, void *user) {
    if (!ctx || !buf || !handler) return TMK_ERR_SHORT_BUF;
    if (len < TMK_SESSION_HDR_SIZE) return TMK_ERR_SHORT_BUF;

    /* Parse session header */
    tmk_session_header_t shdr;
    int hdr_rc = tmk_parse_session_header(buf, len, &shdr);
    if (hdr_rc < 0) return hdr_rc;

    ctx->session_header_valid = true;
    ctx->current_header = shdr;

    size_t offset = (size_t)hdr_rc;
    int frames_processed = 0;

    for (uint16_t i = 0; i < shdr.frame_count && offset < len; i++) {
        tmk_parsed_frame_t frame;
        int frame_rc = tmk_parse_single_frame(buf + offset, len - offset, &frame);
        if (frame_rc < 0) {
            /* Skip to next potential frame boundary */
            offset += TMK_FRAME_HDR_SIZE;
            continue;
        }

        /* Validate session_id matches header */
        if (frame.header.session_id != shdr.session_id) {
            offset += (size_t)frame_rc;
            continue;
        }

        /* Validate sequence is monotonically increasing within session */
        if (frames_processed > 0 && frame.header.sequence <= ctx->global_sequence) {
            offset += (size_t)frame_rc;
            continue;
        }
        ctx->global_sequence = frame.header.sequence;

        /* Dispatch to handler */
        int handle_rc = handler(ctx, &frame, user);
        if (handle_rc < 0) {
            offset += (size_t)frame_rc;
            continue;
        }

        offset += (size_t)frame_rc;
        frames_processed++;
    }

    return frames_processed;
}
