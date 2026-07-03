#ifndef TMK_RETENTION_WATERMARK_H
#define TMK_RETENTION_WATERMARK_H

#include "../../include/tidemark/types.h"

/*
 * Watermark management — tracks high-water and low-water marks
 * for retention evaluation.
 */
uint32_t tmk_retention_high_watermark(tmk_context_t *ctx);
uint32_t tmk_retention_low_watermark(tmk_context_t *ctx);
bool tmk_retention_above_threshold(tmk_context_t *ctx);

#endif
