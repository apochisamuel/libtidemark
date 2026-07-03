#ifndef TMK_INTERNAL_POOL_H
#define TMK_INTERNAL_POOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Arena-style memory pool for allocation-intensive paths.
 * Used by the wire parser for temporary frame storage and by
 * the index manager for node allocation batches.
 *
 * The pool allocates from a contiguous region with bump-pointer
 * semantics. Reset returns the bump pointer to the beginning
 * without freeing the underlying buffer.
 */

#define TMK_POOL_DEFAULT_SIZE (256 * 1024)
#define TMK_POOL_ALIGN 8

typedef struct {
    uint8_t *base;
    size_t   capacity;
    size_t   offset;
    size_t   peak;
    uint32_t alloc_count;
    uint32_t reset_count;
    bool     owned;
} tmk_pool_t;

int  tmk_pool_init(tmk_pool_t *pool, size_t capacity);
int  tmk_pool_init_static(tmk_pool_t *pool, uint8_t *buf, size_t capacity);
void tmk_pool_destroy(tmk_pool_t *pool);
void tmk_pool_reset(tmk_pool_t *pool);

void *tmk_pool_alloc(tmk_pool_t *pool, size_t size);
void *tmk_pool_alloc_aligned(tmk_pool_t *pool, size_t size, size_t align);
void *tmk_pool_calloc(tmk_pool_t *pool, size_t count, size_t size);

size_t tmk_pool_available(const tmk_pool_t *pool);
size_t tmk_pool_used(const tmk_pool_t *pool);
size_t tmk_pool_peak(const tmk_pool_t *pool);
float  tmk_pool_utilization(const tmk_pool_t *pool);

/* Snapshot/restore for nested allocations */
typedef struct {
    size_t offset;
    uint32_t alloc_count;
} tmk_pool_snapshot_t;

tmk_pool_snapshot_t tmk_pool_snapshot(const tmk_pool_t *pool);
void tmk_pool_restore(tmk_pool_t *pool, tmk_pool_snapshot_t snap);

#endif
