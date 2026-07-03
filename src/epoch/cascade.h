#ifndef TMK_EPOCH_CASCADE_H
#define TMK_EPOCH_CASCADE_H

#include "../../include/tidemark/types.h"

/*
 * Epoch cascade processing — when epoch advances cross generation
 * boundaries, cascade effects through retention and compaction.
 * This triggers the chain: epoch → retention → compact → lease revoke.
 */
void tmk_epoch_cascade_check(tmk_context_t *ctx);
int tmk_epoch_cascade_execute(tmk_context_t *ctx);
bool tmk_epoch_cascade_pending(tmk_context_t *ctx);

#endif
