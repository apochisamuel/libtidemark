#ifndef TMK_CURSOR_SEEK_H
#define TMK_CURSOR_SEEK_H

#include "../../include/tidemark/types.h"

/*
 * Cursor seek operations for positioned reads.
 * Supports absolute positioning, relative offset, and
 * key-based seeking through attached index.
 */

typedef enum {
    TMK_SEEK_SET = 0,     /* Absolute position from start */
    TMK_SEEK_CUR = 1,     /* Relative to current position */
    TMK_SEEK_END = 2,     /* Relative to end (negative offset) */
    TMK_SEEK_KEY = 3      /* Key-based seek through index */
} tmk_seek_mode_t;

int tmk_cursor_seek(tmk_context_t *ctx, uint16_t cursor_id,
                    int64_t offset, tmk_seek_mode_t mode);
int tmk_cursor_tell(tmk_context_t *ctx, uint16_t cursor_id,
                    size_t *out_position);
int tmk_cursor_rewind(tmk_context_t *ctx, uint16_t cursor_id);
bool tmk_cursor_at_end(tmk_context_t *ctx, uint16_t cursor_id);
int tmk_cursor_remaining(tmk_context_t *ctx, uint16_t cursor_id,
                         size_t *out_remaining);

/* Batch read: reads up to max_bytes from cursor position */
int tmk_cursor_read_batch(tmk_context_t *ctx, uint16_t cursor_id,
                          uint8_t *out, size_t max_bytes,
                          size_t *bytes_read);

/* Reverse iteration support */
int tmk_cursor_seek_reverse(tmk_context_t *ctx, uint16_t cursor_id);
int tmk_cursor_prev(tmk_context_t *ctx, uint16_t cursor_id);

#endif
