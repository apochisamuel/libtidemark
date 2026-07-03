#ifndef TMK_SEGMENT_SORTED_MERGE_H
#define TMK_SEGMENT_SORTED_MERGE_H

#include "../../include/tidemark/types.h"

/*
 * Sorted merge of two segment data regions.
 *
 * Time-series records within segments are sorted by key (uint32 LE
 * at offset 0 of each 8-byte record).  Merge produces a sorted
 * union of records from both inputs, deduplicating by key and
 * keeping the record with the higher timestamp (uint32 LE at offset 4).
 *
 * This is the core operation for size-tiered compaction.
 */

int tmk_segment_sorted_merge(const uint8_t *data_a, size_t len_a,
                             const uint8_t *data_b, size_t len_b,
                             uint8_t *out, size_t out_cap,
                             size_t *out_len);

/* In-place sort of 8-byte records by key within a segment buffer */
int tmk_segment_sort_records(uint8_t *data, size_t len);

/* Dedup: removes records with duplicate keys, keeping highest timestamp */
int tmk_segment_dedup_records(uint8_t *data, size_t len, size_t *new_len);

/* Binary search for a key within sorted segment data */
int tmk_segment_bsearch_key(const uint8_t *data, size_t len,
                            uint32_t key, size_t *out_offset);

/* Count distinct keys */
int tmk_segment_distinct_keys(const uint8_t *data, size_t len);

#endif
