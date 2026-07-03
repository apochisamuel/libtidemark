#ifndef TMK_WIRE_DIAGNOSTIC_H
#define TMK_WIRE_DIAGNOSTIC_H

#include "../../include/tidemark/types.h"

/*
 * Wire protocol diagnostics — deep inspection and analysis
 * of wire messages for debugging and monitoring.
 */

typedef struct {
    /* Frame type distribution */
    uint32_t session_frames;
    uint32_t channel_frames;
    uint32_t partition_frames;
    uint32_t segment_frames;
    uint32_t data_frames;
    uint32_t index_frames;
    uint32_t cursor_frames;
    uint32_t lease_frames;
    uint32_t epoch_frames;
    uint32_t retention_frames;
    uint32_t compact_frames;
    uint32_t checkpoint_frames;
    uint32_t migrate_frames;
    uint32_t materialize_frames;
    uint32_t unknown_frames;

    /* Subrecord distribution */
    uint32_t data_subrecords;
    uint32_t control_subrecords;
    uint32_t metadata_subrecords;
    uint32_t index_subrecords;
    uint32_t lease_subrecords;
    uint32_t other_subrecords;

    /* Payload statistics */
    uint64_t total_payload_bytes;
    uint32_t avg_payload_size;
    uint32_t max_payload_size;
    uint32_t min_payload_size;

    /* Sequence analysis */
    uint32_t sequence_gaps;
    uint32_t sequence_resets;
    uint32_t max_sequence;

    /* Epoch analysis */
    uint32_t min_epoch;
    uint32_t max_epoch;
    uint32_t epoch_advances;

    /* Owner tokens */
    uint32_t distinct_owners;
    uint32_t distinct_sessions;
    uint32_t distinct_channels;
    uint32_t distinct_partitions;

    /* Error counts */
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t type_errors;
    uint32_t subrec_errors;
} tmk_diagnostic_report_t;

int tmk_diagnostic_analyze_message(const uint8_t *buf, size_t len,
                                   tmk_diagnostic_report_t *report);
int tmk_diagnostic_analyze_frame(const tmk_parsed_frame_t *frame,
                                 tmk_diagnostic_report_t *report);
int tmk_diagnostic_print_summary(const tmk_diagnostic_report_t *report,
                                 char *buf, size_t buf_len);
float tmk_diagnostic_error_rate(const tmk_diagnostic_report_t *report);
int tmk_diagnostic_frame_type_histogram(const tmk_diagnostic_report_t *report,
                                        uint32_t *out_counts, int max_types);

#endif
