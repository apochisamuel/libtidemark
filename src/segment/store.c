/*
 * libtidemark — segment data store operations
 *
 * Low-level read/write operations on segment data buffers.
 * These operate on the segment's actual data pointer, which
 * may be cached elsewhere (cursors, leases).
 */
#include "../internal/context.h"
#include "store.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_segment_write(tmk_context_t *ctx, uint16_t segment_id,
                      const uint8_t *data, size_t len) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;
    if (seg->phase != TMK_PHASE_ACTIVE) return TMK_ERR_NOT_ACTIVE;
    if (!seg->data) return TMK_ERR_BAD_STATE;

    size_t avail = seg->capacity - seg->used;
    size_t to_write = (len <= avail) ? len : avail;

    if (to_write > 0) {
        memcpy(seg->data + seg->used, data, to_write);
        seg->used += to_write;
    }

    return (int)to_write;
}

int tmk_segment_read(tmk_context_t *ctx, uint16_t segment_id,
                     uint8_t *out, size_t offset, size_t len) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;
    if (!seg->data) return TMK_ERR_BAD_STATE;

    if (offset >= seg->used) return 0;
    size_t avail = seg->used - offset;
    size_t to_read = (len <= avail) ? len : avail;

    memcpy(out, seg->data + offset, to_read);
    return (int)to_read;
}

int tmk_segment_seal(tmk_context_t *ctx, uint16_t segment_id) {
    tmk_segment_t *seg = tmk_find_segment(ctx, segment_id);
    if (!seg || !seg->occupied) return TMK_ERR_NOT_FOUND;
    if (seg->phase != TMK_PHASE_ACTIVE) return TMK_ERR_NOT_ACTIVE;
    seg->phase = TMK_PHASE_SEALED;
    return TMK_OK;
}
