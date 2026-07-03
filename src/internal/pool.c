/*
 * libtidemark — arena memory pool
 *
 * Bump-pointer allocator for fast, temporary allocations during
 * frame processing. The pool avoids per-allocation overhead of
 * malloc/free for short-lived objects like parsed frame arrays
 * and subrecord buffers.
 *
 * Alignment is enforced to TMK_POOL_ALIGN (8 bytes) by default.
 * Custom alignment (power of 2) is supported via tmk_pool_alloc_aligned.
 */
#include "pool.h"
#include <stdlib.h>
#include <string.h>

static size_t align_up(size_t val, size_t align) {
    return (val + align - 1) & ~(align - 1);
}

int tmk_pool_init(tmk_pool_t *pool, size_t capacity) {
    if (!pool || capacity == 0) return -1;
    pool->base = (uint8_t *)malloc(capacity);
    if (!pool->base) return -1;
    pool->capacity = capacity;
    pool->offset = 0;
    pool->peak = 0;
    pool->alloc_count = 0;
    pool->reset_count = 0;
    pool->owned = true;
    return 0;
}

int tmk_pool_init_static(tmk_pool_t *pool, uint8_t *buf, size_t capacity) {
    if (!pool || !buf || capacity == 0) return -1;
    pool->base = buf;
    pool->capacity = capacity;
    pool->offset = 0;
    pool->peak = 0;
    pool->alloc_count = 0;
    pool->reset_count = 0;
    pool->owned = false;
    return 0;
}

void tmk_pool_destroy(tmk_pool_t *pool) {
    if (!pool) return;
    if (pool->owned && pool->base) {
        free(pool->base);
    }
    pool->base = NULL;
    pool->capacity = 0;
    pool->offset = 0;
}

void tmk_pool_reset(tmk_pool_t *pool) {
    if (!pool) return;
    pool->offset = 0;
    pool->alloc_count = 0;
    pool->reset_count++;
}

void *tmk_pool_alloc(tmk_pool_t *pool, size_t size) {
    return tmk_pool_alloc_aligned(pool, size, TMK_POOL_ALIGN);
}

void *tmk_pool_alloc_aligned(tmk_pool_t *pool, size_t size, size_t align) {
    if (!pool || !pool->base || size == 0) return NULL;
    if (align == 0 || (align & (align - 1)) != 0) return NULL;

    size_t aligned_offset = align_up(pool->offset, align);
    size_t needed = aligned_offset + size;

    if (needed > pool->capacity) return NULL;

    void *ptr = pool->base + aligned_offset;
    pool->offset = needed;
    pool->alloc_count++;

    if (pool->offset > pool->peak) {
        pool->peak = pool->offset;
    }

    return ptr;
}

void *tmk_pool_calloc(tmk_pool_t *pool, size_t count, size_t size) {
    size_t total = count * size;
    if (total / size != count) return NULL; /* overflow */
    void *ptr = tmk_pool_alloc(pool, total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

size_t tmk_pool_available(const tmk_pool_t *pool) {
    if (!pool) return 0;
    return pool->capacity - pool->offset;
}

size_t tmk_pool_used(const tmk_pool_t *pool) {
    if (!pool) return 0;
    return pool->offset;
}

size_t tmk_pool_peak(const tmk_pool_t *pool) {
    if (!pool) return 0;
    return pool->peak;
}

float tmk_pool_utilization(const tmk_pool_t *pool) {
    if (!pool || pool->capacity == 0) return 0.0f;
    return (float)pool->peak / (float)pool->capacity;
}

tmk_pool_snapshot_t tmk_pool_snapshot(const tmk_pool_t *pool) {
    tmk_pool_snapshot_t snap = {0, 0};
    if (pool) {
        snap.offset = pool->offset;
        snap.alloc_count = pool->alloc_count;
    }
    return snap;
}

void tmk_pool_restore(tmk_pool_t *pool, tmk_pool_snapshot_t snap) {
    if (!pool) return;
    if (snap.offset <= pool->offset) {
        pool->offset = snap.offset;
        pool->alloc_count = snap.alloc_count;
    }
}
