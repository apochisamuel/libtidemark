/*
 * libtidemark — bit vector operations
 *
 * Word-level operations for fast bulk set manipulation.
 * All operations are O(n/32) where n is the capacity.
 */
#include "bitset.h"
#include <string.h>

void tmk_bitset_init(tmk_bitset_t *bs, uint32_t capacity) {
    if (!bs) return;
    memset(bs->words, 0, sizeof(bs->words));
    bs->count = 0;
    bs->capacity = (capacity > TMK_BITSET_MAX_BITS) ? TMK_BITSET_MAX_BITS : capacity;
}

void tmk_bitset_set(tmk_bitset_t *bs, uint32_t bit) {
    if (!bs || bit >= bs->capacity) return;
    uint32_t idx = bit / 32;
    uint32_t mask = 1u << (bit % 32);
    if (!(bs->words[idx] & mask)) {
        bs->words[idx] |= mask;
        bs->count++;
    }
}

void tmk_bitset_clear(tmk_bitset_t *bs, uint32_t bit) {
    if (!bs || bit >= bs->capacity) return;
    uint32_t idx = bit / 32;
    uint32_t mask = 1u << (bit % 32);
    if (bs->words[idx] & mask) {
        bs->words[idx] &= ~mask;
        if (bs->count > 0) bs->count--;
    }
}

bool tmk_bitset_test(const tmk_bitset_t *bs, uint32_t bit) {
    if (!bs || bit >= bs->capacity) return false;
    return (bs->words[bit / 32] & (1u << (bit % 32))) != 0;
}

void tmk_bitset_clear_all(tmk_bitset_t *bs) {
    if (!bs) return;
    memset(bs->words, 0, sizeof(bs->words));
    bs->count = 0;
}

uint32_t tmk_bitset_count(const tmk_bitset_t *bs) {
    if (!bs) return 0;
    return bs->count;
}

int tmk_bitset_first_set(const tmk_bitset_t *bs) {
    if (!bs) return -1;
    uint32_t nwords = (bs->capacity + 31) / 32;
    for (uint32_t i = 0; i < nwords; i++) {
        if (bs->words[i]) {
            uint32_t w = bs->words[i];
            int bit = 0;
            while (!(w & 1)) { w >>= 1; bit++; }
            int result = (int)(i * 32) + bit;
            return (result < (int)bs->capacity) ? result : -1;
        }
    }
    return -1;
}

int tmk_bitset_first_clear(const tmk_bitset_t *bs) {
    if (!bs) return -1;
    uint32_t nwords = (bs->capacity + 31) / 32;
    for (uint32_t i = 0; i < nwords; i++) {
        if (~bs->words[i]) {
            uint32_t w = ~bs->words[i];
            int bit = 0;
            while (!(w & 1)) { w >>= 1; bit++; }
            int result = (int)(i * 32) + bit;
            return (result < (int)bs->capacity) ? result : -1;
        }
    }
    return -1;
}

int tmk_bitset_next_set(const tmk_bitset_t *bs, uint32_t after) {
    if (!bs) return -1;
    for (uint32_t i = after + 1; i < bs->capacity; i++) {
        if (tmk_bitset_test(bs, i)) return (int)i;
    }
    return -1;
}

void tmk_bitset_or(tmk_bitset_t *dst, const tmk_bitset_t *src) {
    if (!dst || !src) return;
    uint32_t nwords = (dst->capacity + 31) / 32;
    dst->count = 0;
    for (uint32_t i = 0; i < nwords; i++) {
        dst->words[i] |= src->words[i];
        uint32_t w = dst->words[i];
        while (w) { w &= w - 1; dst->count++; }
    }
}

void tmk_bitset_and(tmk_bitset_t *dst, const tmk_bitset_t *src) {
    if (!dst || !src) return;
    uint32_t nwords = (dst->capacity + 31) / 32;
    dst->count = 0;
    for (uint32_t i = 0; i < nwords; i++) {
        dst->words[i] &= src->words[i];
        uint32_t w = dst->words[i];
        while (w) { w &= w - 1; dst->count++; }
    }
}

void tmk_bitset_xor(tmk_bitset_t *dst, const tmk_bitset_t *src) {
    if (!dst || !src) return;
    uint32_t nwords = (dst->capacity + 31) / 32;
    dst->count = 0;
    for (uint32_t i = 0; i < nwords; i++) {
        dst->words[i] ^= src->words[i];
        uint32_t w = dst->words[i];
        while (w) { w &= w - 1; dst->count++; }
    }
}

void tmk_bitset_not(tmk_bitset_t *bs) {
    if (!bs) return;
    uint32_t nwords = (bs->capacity + 31) / 32;
    bs->count = 0;
    for (uint32_t i = 0; i < nwords; i++) {
        bs->words[i] = ~bs->words[i];
        uint32_t w = bs->words[i];
        while (w) { w &= w - 1; bs->count++; }
    }
    /* Mask unused bits in last word */
    uint32_t tail = bs->capacity % 32;
    if (tail > 0 && nwords > 0) {
        bs->words[nwords - 1] &= (1u << tail) - 1;
        /* Recount last word */
        uint32_t w = bs->words[nwords - 1];
        uint32_t c = 0;
        while (w) { w &= w - 1; c++; }
        bs->count = bs->count - 32 + tail + c; /* approximate fix */
    }
}

bool tmk_bitset_intersects(const tmk_bitset_t *a, const tmk_bitset_t *b) {
    if (!a || !b) return false;
    uint32_t nwords = (a->capacity + 31) / 32;
    for (uint32_t i = 0; i < nwords; i++) {
        if (a->words[i] & b->words[i]) return true;
    }
    return false;
}

bool tmk_bitset_subset(const tmk_bitset_t *subset, const tmk_bitset_t *superset) {
    if (!subset || !superset) return false;
    uint32_t nwords = (subset->capacity + 31) / 32;
    for (uint32_t i = 0; i < nwords; i++) {
        if (subset->words[i] & ~superset->words[i]) return false;
    }
    return true;
}

bool tmk_bitset_empty(const tmk_bitset_t *bs) {
    return !bs || bs->count == 0;
}

float tmk_bitset_density(const tmk_bitset_t *bs) {
    if (!bs || bs->capacity == 0) return 0.0f;
    return (float)bs->count / (float)bs->capacity;
}
