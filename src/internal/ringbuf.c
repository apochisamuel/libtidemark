/*
 * libtidemark — ring buffer for inter-subsystem event queuing
 *
 * Single-producer single-consumer ring buffer using head/tail
 * indices with masking.  Overflow causes events to be dropped
 * (counted in the dropped field for diagnostics).
 */
#include "ringbuf.h"
#include <string.h>

void tmk_ringbuf_init(tmk_ringbuf_t *rb) {
    if (!rb) return;
    memset(rb->entries, 0, sizeof(rb->entries));
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->dropped = 0;
    rb->capacity = TMK_RINGBUF_MAX_ENTRIES;
}

int tmk_ringbuf_push(tmk_ringbuf_t *rb, const tmk_event_t *event) {
    if (!rb || !event) return -1;

    if (rb->count >= rb->capacity) {
        rb->dropped++;
        return -1;
    }

    rb->entries[rb->head % rb->capacity] = *event;
    rb->head++;
    rb->count++;
    return 0;
}

int tmk_ringbuf_pop(tmk_ringbuf_t *rb, tmk_event_t *out) {
    if (!rb || !out) return -1;
    if (rb->count == 0) return -1;

    *out = rb->entries[rb->tail % rb->capacity];
    rb->tail++;
    rb->count--;
    return 0;
}

int tmk_ringbuf_peek(const tmk_ringbuf_t *rb, tmk_event_t *out) {
    if (!rb || !out) return -1;
    if (rb->count == 0) return -1;

    *out = rb->entries[rb->tail % rb->capacity];
    return 0;
}

bool tmk_ringbuf_empty(const tmk_ringbuf_t *rb) {
    return !rb || rb->count == 0;
}

bool tmk_ringbuf_full(const tmk_ringbuf_t *rb) {
    return rb && rb->count >= rb->capacity;
}

uint32_t tmk_ringbuf_count(const tmk_ringbuf_t *rb) {
    return rb ? rb->count : 0;
}

void tmk_ringbuf_clear(tmk_ringbuf_t *rb) {
    if (!rb) return;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

int tmk_ringbuf_drain(tmk_ringbuf_t *rb, tmk_event_t *out, uint32_t max) {
    if (!rb || !out) return -1;

    uint32_t drained = 0;
    while (drained < max && rb->count > 0) {
        out[drained] = rb->entries[rb->tail % rb->capacity];
        rb->tail++;
        rb->count--;
        drained++;
    }

    return (int)drained;
}

uint32_t tmk_ringbuf_dropped(const tmk_ringbuf_t *rb) {
    return rb ? rb->dropped : 0;
}
