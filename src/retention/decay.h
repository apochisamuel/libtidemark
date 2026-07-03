#ifndef TMK_RETENTION_DECAY_H
#define TMK_RETENTION_DECAY_H

#include "../../include/tidemark/types.h"

/*
 * Retention decay curve evaluation.
 *
 * Different decay models determine how segment retention priority
 * changes with epoch age:
 *
 * - Linear:      priority = 1.0 - (age / ttl)
 * - Exponential: priority = exp(-lambda * age)
 * - Step:        priority = 1.0 if age < ttl, 0.0 otherwise
 * - Logarithmic: priority = 1.0 - log(1 + age) / log(1 + ttl)
 */

typedef enum {
    TMK_DECAY_LINEAR = 0,
    TMK_DECAY_EXPONENTIAL = 1,
    TMK_DECAY_STEP = 2,
    TMK_DECAY_LOGARITHMIC = 3
} tmk_decay_model_t;

typedef struct {
    tmk_decay_model_t model;
    double            ttl;
    double            lambda;       /* for exponential */
    double            threshold;    /* below this, segment is expired */
} tmk_decay_config_t;

int  tmk_decay_init(tmk_decay_config_t *cfg, tmk_decay_model_t model,
                    double ttl, double threshold);
double tmk_decay_evaluate(const tmk_decay_config_t *cfg, double age);
bool tmk_decay_is_expired(const tmk_decay_config_t *cfg, double age);
int  tmk_decay_evaluate_segment(tmk_context_t *ctx,
                                const tmk_decay_config_t *cfg,
                                uint16_t segment_id, double *out_priority);
int  tmk_decay_evaluate_partition(tmk_context_t *ctx,
                                  const tmk_decay_config_t *cfg,
                                  uint16_t partition_id,
                                  double *out_avg_priority);
int  tmk_decay_find_expired(tmk_context_t *ctx,
                            const tmk_decay_config_t *cfg,
                            uint16_t *out_ids, uint16_t max_count,
                            uint16_t *out_count);

#endif
