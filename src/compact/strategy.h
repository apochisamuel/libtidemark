#ifndef TMK_COMPACT_STRATEGY_H
#define TMK_COMPACT_STRATEGY_H

#include "../../include/tidemark/types.h"

/*
 * Compaction strategy selection and scoring.
 *
 * Multiple strategies compete for compaction budget:
 * - Size-tiered: merge segments of similar size
 * - Leveled: merge into progressively larger levels
 * - Time-windowed: merge by epoch range
 * - Tombstone-driven: prioritize segments with high tombstone density
 *
 * Each strategy computes a score; the highest-scoring candidate wins.
 */

typedef enum {
    TMK_COMPACT_SIZE_TIERED = 0,
    TMK_COMPACT_LEVELED     = 1,
    TMK_COMPACT_TIME_WINDOW = 2,
    TMK_COMPACT_TOMBSTONE   = 3
} tmk_compact_strategy_t;

typedef struct {
    tmk_compact_strategy_t strategy;
    uint16_t candidate_a;
    uint16_t candidate_b;
    float    score;
    uint32_t estimated_savings;
    uint32_t epoch_range;
} tmk_compact_candidate_t;

int tmk_compact_evaluate_candidates(tmk_context_t *ctx,
                                    tmk_compact_candidate_t *out,
                                    int max_candidates);
int tmk_compact_score_size_tiered(tmk_context_t *ctx,
                                  uint16_t seg_a, uint16_t seg_b,
                                  float *out_score);
int tmk_compact_score_leveled(tmk_context_t *ctx,
                              uint16_t segment_id, float *out_score);
int tmk_compact_score_time_window(tmk_context_t *ctx,
                                  uint16_t segment_id, float *out_score);
int tmk_compact_score_tombstone(tmk_context_t *ctx,
                                uint16_t segment_id, float *out_score);
tmk_compact_strategy_t tmk_compact_select_strategy(tmk_context_t *ctx);

#endif
