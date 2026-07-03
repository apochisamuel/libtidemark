/*
 * libtidemark — session lifecycle fuzz harness
 *
 * Processes fuzzed byte streams through the session processing API.
 * Supports multi-message: first 2 bytes encode split point.
 */
#include "../include/tidemark/tidemark.h"
#include <string.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return 0;

    tmk_context_t *ctx = tmk_context_create();
    if (!ctx) return 0;

    uint16_t split = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    const uint8_t *payload = data + 2;
    size_t payload_len = size - 2;

    if (split >= payload_len) {
        tmk_session_process(ctx, payload, payload_len);
    } else {
        tmk_session_process(ctx, payload, split);
        tmk_session_process(ctx, payload + split, payload_len - split);
    }

    tmk_context_destroy(ctx);
    return 0;
}
