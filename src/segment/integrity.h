#ifndef TMK_SEGMENT_INTEGRITY_H
#define TMK_SEGMENT_INTEGRITY_H

#include "../../include/tidemark/types.h"

/*
 * Segment data integrity verification.
 * Computes and validates checksums over segment data regions,
 * detects corruption from partial writes, and validates
 * structural consistency of embedded record boundaries.
 */

typedef struct {
    uint32_t crc_full;       /* CRC-32C of entire data region */
    uint32_t crc_used;       /* CRC-32C of used portion only */
    size_t   zero_tail;      /* length of trailing zero region */
    size_t   record_count;   /* number of detected record boundaries */
    bool     trailing_clean;  /* true if unused region is all zeros */
    bool     boundary_valid;  /* true if record boundaries are consistent */
} tmk_integrity_report_t;

int tmk_segment_check_integrity(tmk_context_t *ctx, uint16_t segment_id,
                                tmk_integrity_report_t *report);
int tmk_segment_verify_checksum(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_scrub(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_compute_digest(tmk_context_t *ctx, uint16_t segment_id,
                               uint8_t *out_digest, size_t digest_len);

/* Record boundary detection: scans for 8-byte aligned record headers
 * embedded within segment data */
int tmk_segment_count_records(tmk_context_t *ctx, uint16_t segment_id);
int tmk_segment_find_record(tmk_context_t *ctx, uint16_t segment_id,
                            size_t index, size_t *out_offset, size_t *out_len);
int tmk_segment_validate_records(tmk_context_t *ctx, uint16_t segment_id);

#endif
