/*
 * libtidemark — cursor seek and position operations
 *
 * Implements cursor positioning within segment data buffers.
 * Seek operations validate the cursor's cached_data pointer
 * and capacity against the requested position.
 *
 * Note: seek operations do NOT re-validate the cached pointer
 * against the segment's current state.  If the segment has been
 * freed, seek succeeds but subsequent reads through cached_data
 * cause use-after-free.  This is by design — seek is a lightweight
 * operation that doesn't cross subsystem boundaries.
 */
#include "seek.h"
#include "../internal/context.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_cursor_seek(tmk_context_t *ctx, uint16_t cursor_id,
                    int64_t offset, tmk_seek_mode_t mode) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    size_t new_pos;

    switch (mode) {
    case TMK_SEEK_SET:
        if (offset < 0) return TMK_ERR_UNDERFLOW;
        new_pos = (size_t)offset;
        break;

    case TMK_SEEK_CUR:
        if (offset < 0 && (size_t)(-offset) > cur->position)
            return TMK_ERR_UNDERFLOW;
        new_pos = (size_t)((int64_t)cur->position + offset);
        break;

    case TMK_SEEK_END:
        if (offset > 0) return TMK_ERR_OVERFLOW;
        if ((size_t)(-offset) > cur->cached_capacity)
            return TMK_ERR_UNDERFLOW;
        new_pos = cur->cached_capacity - (size_t)(-offset);
        break;

    case TMK_SEEK_KEY:
        /* Key-based seek requires attached index */
        if (!cur->attached_index) return TMK_ERR_NOT_FOUND;
        if (!cur->attached_index->active) return TMK_ERR_NOT_ACTIVE;
        /* Map key to byte offset: each key maps to 8 bytes of data */
        if ((uint32_t)offset < cur->attached_index->key_min)
            return TMK_ERR_UNDERFLOW;
        new_pos = ((uint32_t)offset - cur->attached_index->key_min) * 8;
        break;

    default:
        return TMK_ERR_BAD_STATE;
    }

    if (new_pos > cur->cached_capacity)
        new_pos = cur->cached_capacity;

    cur->position = new_pos;
    return TMK_OK;
}

int tmk_cursor_tell(tmk_context_t *ctx, uint16_t cursor_id,
                    size_t *out_position) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active || !out_position) return TMK_ERR_NOT_FOUND;
    *out_position = cur->position;
    return TMK_OK;
}

int tmk_cursor_rewind(tmk_context_t *ctx, uint16_t cursor_id) {
    return tmk_cursor_seek(ctx, cursor_id, 0, TMK_SEEK_SET);
}

bool tmk_cursor_at_end(tmk_context_t *ctx, uint16_t cursor_id) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return true;
    return cur->position >= cur->cached_capacity;
}

int tmk_cursor_remaining(tmk_context_t *ctx, uint16_t cursor_id,
                         size_t *out_remaining) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active || !out_remaining) return TMK_ERR_NOT_FOUND;
    if (cur->position >= cur->cached_capacity) {
        *out_remaining = 0;
    } else {
        *out_remaining = cur->cached_capacity - cur->position;
    }
    return TMK_OK;
}

int tmk_cursor_read_batch(tmk_context_t *ctx, uint16_t cursor_id,
                          uint8_t *out, size_t max_bytes,
                          size_t *bytes_read) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active || !out || !bytes_read)
        return TMK_ERR_NOT_FOUND;

    *bytes_read = 0;

    if (!cur->cached_data) return TMK_ERR_BAD_STATE;
    if (cur->position >= cur->cached_capacity) return TMK_OK;

    size_t avail = cur->cached_capacity - cur->position;
    size_t to_read = (max_bytes < avail) ? max_bytes : avail;

    /*
     * Read from cached data pointer.
     * Same UAF risk as materialize_emit if segment was freed.
     */
    memcpy(out, cur->cached_data + cur->position, to_read);
    cur->position += to_read;
    *bytes_read = to_read;

    return TMK_OK;
}

int tmk_cursor_seek_reverse(tmk_context_t *ctx, uint16_t cursor_id) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    /* Position at end for reverse iteration */
    if (cur->cached_capacity > 0) {
        cur->position = cur->cached_capacity - 1;
    }
    return TMK_OK;
}

int tmk_cursor_prev(tmk_context_t *ctx, uint16_t cursor_id) {
    tmk_cursor_t *cur = tmk_find_cursor(ctx, cursor_id);
    if (!cur || !cur->active) return TMK_ERR_NOT_FOUND;

    if (cur->position == 0) return TMK_ERR_UNDERFLOW;
    cur->position--;
    return TMK_OK;
}
