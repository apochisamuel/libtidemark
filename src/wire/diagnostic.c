/*
 * libtidemark — wire protocol diagnostics
 *
 * Deep inspection of wire messages for debugging and monitoring.
 * Provides frame type distribution, payload statistics, sequence
 * analysis, and error counting.
 *
 * The analyzer processes the full message without dispatching to
 * subsystem handlers — it only does structural analysis of the
 * wire protocol itself.
 */
#include "diagnostic.h"
#include "header.h"
#include "frame.h"
#include "subrecord.h"
#include "parser.h"
#include "../../include/tidemark/error.h"
#include <string.h>
#include <stdio.h>

static void classify_frame(uint16_t ft, tmk_diagnostic_report_t *r) {
    if (ft >= TMK_FRAME_SESSION_OPEN && ft <= TMK_FRAME_SESSION_CLOSE)
        r->session_frames++;
    else if (ft >= TMK_FRAME_CHANNEL_REGISTER && ft <= TMK_FRAME_CHANNEL_CACHE_LEASE)
        r->channel_frames++;
    else if (ft >= TMK_FRAME_PARTITION_CREATE && ft <= TMK_FRAME_PARTITION_CACHE_EPOCH)
        r->partition_frames++;
    else if (ft >= TMK_FRAME_SEGMENT_CREATE && ft <= TMK_FRAME_SEGMENT_META_INIT)
        r->segment_frames++;
    else if (ft >= TMK_FRAME_DATA_INGEST && ft <= TMK_FRAME_DATA_INGEST_IXWR)
        r->data_frames++;
    else if (ft >= TMK_FRAME_INDEX_REGISTER && ft <= TMK_FRAME_INDEX_REG_CAPACITY)
        r->index_frames++;
    else if (ft >= TMK_FRAME_CURSOR_OPEN && ft <= TMK_FRAME_CURSOR_BIND_LEASE)
        r->cursor_frames++;
    else if (ft >= TMK_FRAME_LEASE_ACQUIRE && ft <= TMK_FRAME_LEASE_BIND_META)
        r->lease_frames++;
    else if (ft == TMK_FRAME_EPOCH_ADVANCE)
        r->epoch_frames++;
    else if (ft >= TMK_FRAME_RETENTION_TRIGGER && ft <= TMK_FRAME_RETENTION_REINDEX)
        r->retention_frames++;
    else if (ft >= TMK_FRAME_COMPACT_SCHEDULE && ft <= TMK_FRAME_COMPACT_DEFER_DRAIN)
        r->compact_frames++;
    else if (ft >= TMK_FRAME_CHECKPOINT_CREATE && ft <= TMK_FRAME_CHECKPOINT_RESTORE)
        r->checkpoint_frames++;
    else if (ft >= TMK_FRAME_MIGRATE_BEGIN && ft <= TMK_FRAME_MIGRATE_ENUMERATE)
        r->migrate_frames++;
    else if (ft >= TMK_FRAME_MATERIALIZE_EMIT && ft <= TMK_FRAME_MATERIALIZE_ISCAN)
        r->materialize_frames++;
    else
        r->unknown_frames++;
}

static void classify_subrecord(uint16_t kind, tmk_diagnostic_report_t *r) {
    switch (kind) {
    case TMK_SUB_DATA:
        r->data_subrecords++; break;
    case TMK_SUB_CONTROL:
        r->control_subrecords++; break;
    case TMK_SUB_METADATA:
        r->metadata_subrecords++; break;
    case TMK_SUB_INDEX_ENTRY:
        r->index_subrecords++; break;
    case TMK_SUB_LEASE_TOKEN:
        r->lease_subrecords++; break;
    default:
        r->other_subrecords++; break;
    }
}

int tmk_diagnostic_analyze_frame(const tmk_parsed_frame_t *frame,
                                 tmk_diagnostic_report_t *report) {
    if (!frame || !report) return TMK_ERR_SHORT_BUF;

    classify_frame(frame->header.frame_type, report);

    /* Payload statistics */
    uint32_t payload = frame->header.length - TMK_FRAME_HDR_SIZE;
    report->total_payload_bytes += payload;
    if (payload > report->max_payload_size)
        report->max_payload_size = payload;
    if (report->min_payload_size == 0 || payload < report->min_payload_size)
        report->min_payload_size = payload;

    /* Sequence tracking */
    if (frame->header.sequence > report->max_sequence) {
        if (report->max_sequence > 0 &&
            frame->header.sequence > report->max_sequence + 1) {
            report->sequence_gaps++;
        }
        report->max_sequence = frame->header.sequence;
    } else if (frame->header.sequence < report->max_sequence) {
        report->sequence_resets++;
    }

    /* Epoch tracking */
    if (frame->header.frame_type == TMK_FRAME_EPOCH_ADVANCE) {
        report->epoch_advances++;
    }

    /* Subrecord analysis */
    for (uint16_t i = 0; i < frame->parsed_sub_count; i++) {
        classify_subrecord(frame->subrecords[i].kind, report);

        uint32_t ep = frame->subrecords[i].local_epoch;
        if (ep > 0) {
            if (report->min_epoch == 0 || ep < report->min_epoch)
                report->min_epoch = ep;
            if (ep > report->max_epoch)
                report->max_epoch = ep;
        }
    }

    return TMK_OK;
}

int tmk_diagnostic_analyze_message(const uint8_t *buf, size_t len,
                                   tmk_diagnostic_report_t *report) {
    if (!buf || !report) return TMK_ERR_SHORT_BUF;
    memset(report, 0, sizeof(*report));

    if (len < TMK_SESSION_HDR_SIZE) {
        report->length_errors++;
        return TMK_ERR_SHORT_BUF;
    }

    tmk_session_header_t shdr;
    int rc = tmk_parse_session_header(buf, len, &shdr);
    if (rc < 0) {
        report->crc_errors++;
        return rc;
    }

    report->distinct_sessions = 1; /* at least the session header */

    /* Track unique IDs */
    uint32_t seen_owners[64];
    uint16_t seen_channels[64];
    uint16_t seen_partitions[64];
    int n_owners = 0, n_channels = 0, n_partitions = 0;

    size_t offset = (size_t)rc;
    int frames = 0;

    for (uint16_t i = 0; i < shdr.frame_count && offset < len; i++) {
        tmk_parsed_frame_t frame;
        int frc = tmk_parse_single_frame(buf + offset, len - offset, &frame);
        if (frc < 0) {
            if (frc == TMK_ERR_BAD_CRC) report->crc_errors++;
            else if (frc == TMK_ERR_BAD_FRAME_LEN) report->length_errors++;
            else if (frc == TMK_ERR_BAD_FRAME_TYPE) report->type_errors++;
            else if (frc == TMK_ERR_BAD_SUBREC_LEN || frc == TMK_ERR_BAD_SUBREC_KIND)
                report->subrec_errors++;
            offset += TMK_FRAME_HDR_SIZE;
            continue;
        }

        tmk_diagnostic_analyze_frame(&frame, report);

        /* Track unique owners */
        if (frame.header.owner_token != 0) {
            int found = 0;
            for (int j = 0; j < n_owners; j++) {
                if (seen_owners[j] == frame.header.owner_token) { found = 1; break; }
            }
            if (!found && n_owners < 64) {
                seen_owners[n_owners++] = frame.header.owner_token;
            }
        }

        /* Track unique channels */
        if (frame.header.channel_id != 0) {
            int found = 0;
            for (int j = 0; j < n_channels; j++) {
                if (seen_channels[j] == frame.header.channel_id) { found = 1; break; }
            }
            if (!found && n_channels < 64) {
                seen_channels[n_channels++] = frame.header.channel_id;
            }
        }

        /* Track unique partitions */
        if (frame.header.partition_id != 0) {
            int found = 0;
            for (int j = 0; j < n_partitions; j++) {
                if (seen_partitions[j] == frame.header.partition_id) { found = 1; break; }
            }
            if (!found && n_partitions < 64) {
                seen_partitions[n_partitions++] = frame.header.partition_id;
            }
        }

        offset += (size_t)frc;
        frames++;
    }

    report->distinct_owners = (uint32_t)n_owners;
    report->distinct_channels = (uint32_t)n_channels;
    report->distinct_partitions = (uint32_t)n_partitions;

    /* Compute average payload */
    if (frames > 0) {
        report->avg_payload_size = (uint32_t)(report->total_payload_bytes / (uint64_t)frames);
    }

    return frames;
}

int tmk_diagnostic_print_summary(const tmk_diagnostic_report_t *report,
                                 char *buf, size_t buf_len) {
    if (!report || !buf || buf_len < 256) return TMK_ERR_SHORT_BUF;

    int n = snprintf(buf, buf_len,
        "Frames: sess=%u ch=%u part=%u seg=%u data=%u idx=%u "
        "cur=%u lease=%u epoch=%u ret=%u compact=%u cp=%u mig=%u mat=%u unk=%u\n"
        "Payload: total=%llu avg=%u min=%u max=%u\n"
        "Seq: gaps=%u resets=%u max=%u\n"
        "Epoch: min=%u max=%u advances=%u\n"
        "IDs: owners=%u sessions=%u channels=%u partitions=%u\n"
        "Errors: crc=%u len=%u type=%u subrec=%u\n",
        report->session_frames, report->channel_frames,
        report->partition_frames, report->segment_frames,
        report->data_frames, report->index_frames,
        report->cursor_frames, report->lease_frames,
        report->epoch_frames, report->retention_frames,
        report->compact_frames, report->checkpoint_frames,
        report->migrate_frames, report->materialize_frames,
        report->unknown_frames,
        (unsigned long long)report->total_payload_bytes,
        report->avg_payload_size,
        report->min_payload_size, report->max_payload_size,
        report->sequence_gaps, report->sequence_resets,
        report->max_sequence,
        report->min_epoch, report->max_epoch, report->epoch_advances,
        report->distinct_owners, report->distinct_sessions,
        report->distinct_channels, report->distinct_partitions,
        report->crc_errors, report->length_errors,
        report->type_errors, report->subrec_errors);

    return (n < (int)buf_len) ? n : (int)buf_len;
}

float tmk_diagnostic_error_rate(const tmk_diagnostic_report_t *report) {
    if (!report) return 0.0f;
    uint32_t total = report->session_frames + report->channel_frames +
        report->partition_frames + report->segment_frames +
        report->data_frames + report->index_frames +
        report->cursor_frames + report->lease_frames +
        report->epoch_frames + report->retention_frames +
        report->compact_frames + report->checkpoint_frames +
        report->migrate_frames + report->materialize_frames +
        report->unknown_frames;
    uint32_t errors = report->crc_errors + report->length_errors +
        report->type_errors + report->subrec_errors;

    if (total + errors == 0) return 0.0f;
    return (float)errors / (float)(total + errors);
}

int tmk_diagnostic_frame_type_histogram(const tmk_diagnostic_report_t *report,
                                        uint32_t *out_counts, int max_types) {
    if (!report || !out_counts || max_types < 15) return TMK_ERR_SHORT_BUF;
    memset(out_counts, 0, (size_t)max_types * sizeof(uint32_t));

    out_counts[0] = report->session_frames;
    out_counts[1] = report->channel_frames;
    out_counts[2] = report->partition_frames;
    out_counts[3] = report->segment_frames;
    out_counts[4] = report->data_frames;
    out_counts[5] = report->index_frames;
    out_counts[6] = report->cursor_frames;
    out_counts[7] = report->lease_frames;
    out_counts[8] = report->epoch_frames;
    out_counts[9] = report->retention_frames;
    out_counts[10] = report->compact_frames;
    out_counts[11] = report->checkpoint_frames;
    out_counts[12] = report->migrate_frames;
    out_counts[13] = report->materialize_frames;
    out_counts[14] = report->unknown_frames;

    return 15;
}
