/*
 * libtidemark — channel ingestion fuzz harness
 *
 * Processes fuzzed byte streams through the channel ingestion API.
 * Supports multi-message processing: the first 2 bytes encode the
 * length of the first message.  This enables testing cross-session
 * state interactions (e.g., session close followed by new session
 * accessing stale cached state).
 *
 * The context persists across messages, so state from message 1
 * (like cached partition epoch pointers) is visible to message 2.
 */
#include "../include/tidemark/tidemark.h"
#include <string.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;

    tmk_context_t *ctx = tmk_context_create();
    if (!ctx) return 0;

    /* First 2 bytes = split point for multi-message */
    uint16_t split = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    const uint8_t *payload = data + 2;
    size_t payload_len = size - 2;

    if (split >= payload_len) {
        /* Single message */
        tmk_channel_ingest(ctx, payload, payload_len);
    } else {
        /* Two messages — context persists across both */
        tmk_channel_ingest(ctx, payload, split);
        tmk_channel_ingest(ctx, payload + split, payload_len - split);
    }

    tmk_context_destroy(ctx);
    return 0;
}
