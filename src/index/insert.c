/*
 * libtidemark — index entry insertion
 *
 * Inserts a key-value entry into an index node by writing to the
 * underlying segment.
 */
#include "../internal/context.h"
#include "insert.h"
#include "../segment/store.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_index_insert(tmk_context_t *ctx, uint16_t index_id,
                     uint32_t key, const uint8_t *value, size_t len) {
    tmk_index_node_t *idx = tmk_find_index_node(ctx, index_id);
    if (!idx || !idx->active) return TMK_ERR_NOT_FOUND;

    /* Update key range */
    if (key < idx->key_min) idx->key_min = key;
    if (key > idx->key_max) idx->key_max = key;

    /* Write to underlying segment */
    if (value && len > 0) {
        int rc = tmk_segment_write(ctx, idx->segment_id, value, len);
        if (rc < 0) return rc;
    }

    idx->child_count++;
    return TMK_OK;
}
