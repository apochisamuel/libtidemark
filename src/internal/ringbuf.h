#ifndef TMK_INTERNAL_RINGBUF_H
#define TMK_INTERNAL_RINGBUF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Lock-free ring buffer for event queuing between subsystems.
 * Used by the epoch cascade to queue retention and compaction events.
 */
#define TMK_RINGBUF_MAX_ENTRIES 128

typedef struct {
    uint16_t type;
    uint16_t source_id;
    uint16_t target_id;
    uint16_t flags;
    uint32_t epoch;
    uint32_t generation;
    uint32_t payload;
} tmk_event_t;

typedef struct {
    tmk_event_t entries[TMK_RINGBUF_MAX_ENTRIES];
    uint32_t    head;
    uint32_t    tail;
    uint32_t    count;
    uint32_t    dropped;
    uint32_t    capacity;
} tmk_ringbuf_t;

void tmk_ringbuf_init(tmk_ringbuf_t *rb);
int  tmk_ringbuf_push(tmk_ringbuf_t *rb, const tmk_event_t *event);
int  tmk_ringbuf_pop(tmk_ringbuf_t *rb, tmk_event_t *out);
int  tmk_ringbuf_peek(const tmk_ringbuf_t *rb, tmk_event_t *out);
bool tmk_ringbuf_empty(const tmk_ringbuf_t *rb);
bool tmk_ringbuf_full(const tmk_ringbuf_t *rb);
uint32_t tmk_ringbuf_count(const tmk_ringbuf_t *rb);
void tmk_ringbuf_clear(tmk_ringbuf_t *rb);
int  tmk_ringbuf_drain(tmk_ringbuf_t *rb, tmk_event_t *out, uint32_t max);
uint32_t tmk_ringbuf_dropped(const tmk_ringbuf_t *rb);

#endif
