/*
 * libtidemark — hash and bloom filter utilities
 *
 * FNV-1a hash for general byte sequences, plus integer-specific
 * mixing functions optimized for the typical ID distributions
 * seen in session, channel, and partition identifiers.
 *
 * The bloom filter provides probabilistic tombstone existence
 * checks to avoid unnecessary linear scans during compaction
 * sweep operations.
 */
#include "hash.h"
#include <string.h>

/* FNV-1a constants for 32-bit */
#define FNV_OFFSET 0x811C9DC5u
#define FNV_PRIME  0x01000193u

uint32_t tmk_hash_fnv1a(const uint8_t *data, size_t len) {
    uint32_t hash = FNV_OFFSET;
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

uint32_t tmk_hash_u32(uint32_t key) {
    /* Wang integer hash */
    key = ~key + (key << 15);
    key ^= (key >> 12);
    key += (key << 2);
    key ^= (key >> 4);
    key *= 2057;
    key ^= (key >> 16);
    return key;
}

uint32_t tmk_hash_u16(uint16_t key) {
    return tmk_hash_u32((uint32_t)key);
}

uint64_t tmk_hash_combine(uint64_t h1, uint64_t h2) {
    h1 ^= h2 + 0x9E3779B97F4A7C15ULL + (h1 << 12) + (h1 >> 4);
    return h1;
}

uint32_t tmk_hash_bytes(const void *data, size_t len, uint32_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t hash = seed ^ (uint32_t)len;

    while (len >= 4) {
        uint32_t k;
        memcpy(&k, p, 4);
        k *= 0xCC9E2D51u;
        k = (k << 15) | (k >> 17);
        k *= 0x1B873593u;
        hash ^= k;
        hash = (hash << 13) | (hash >> 19);
        hash = hash * 5 + 0xE6546B64u;
        p += 4;
        len -= 4;
    }

    uint32_t tail = 0;
    switch (len) {
    case 3: tail ^= (uint32_t)p[2] << 16; /* fallthrough */
    case 2: tail ^= (uint32_t)p[1] << 8;  /* fallthrough */
    case 1: tail ^= (uint32_t)p[0];
            tail *= 0xCC9E2D51u;
            tail = (tail << 15) | (tail >> 17);
            tail *= 0x1B873593u;
            hash ^= tail;
    }

    hash ^= (uint32_t)(len + (p - (const uint8_t *)data));
    hash ^= hash >> 16;
    hash *= 0x85EBCA6Bu;
    hash ^= hash >> 13;
    hash *= 0xC2B2AE35u;
    hash ^= hash >> 16;

    return hash;
}

void tmk_bloom_init(tmk_bloom_t *bloom) {
    if (!bloom) return;
    memset(bloom->bits, 0, sizeof(bloom->bits));
    bloom->count = 0;
}

static void bloom_set_bit(tmk_bloom_t *bloom, uint32_t bit) {
    uint32_t idx = bit / 32;
    uint32_t mask = 1u << (bit % 32);
    if (idx < TMK_BLOOM_WORDS) {
        bloom->bits[idx] |= mask;
    }
}

static int bloom_test_bit(const tmk_bloom_t *bloom, uint32_t bit) {
    uint32_t idx = bit / 32;
    uint32_t mask = 1u << (bit % 32);
    if (idx < TMK_BLOOM_WORDS) {
        return (bloom->bits[idx] & mask) != 0;
    }
    return 0;
}

void tmk_bloom_insert(tmk_bloom_t *bloom, uint32_t key) {
    if (!bloom) return;
    uint32_t h1 = tmk_hash_u32(key) % TMK_BLOOM_BITS;
    uint32_t h2 = tmk_hash_u32(key ^ 0xDEADBEEF) % TMK_BLOOM_BITS;
    uint32_t h3 = tmk_hash_u32(key ^ 0xCAFEBABE) % TMK_BLOOM_BITS;
    bloom_set_bit(bloom, h1);
    bloom_set_bit(bloom, h2);
    bloom_set_bit(bloom, h3);
    bloom->count++;
}

int tmk_bloom_check(const tmk_bloom_t *bloom, uint32_t key) {
    if (!bloom) return 0;
    uint32_t h1 = tmk_hash_u32(key) % TMK_BLOOM_BITS;
    uint32_t h2 = tmk_hash_u32(key ^ 0xDEADBEEF) % TMK_BLOOM_BITS;
    uint32_t h3 = tmk_hash_u32(key ^ 0xCAFEBABE) % TMK_BLOOM_BITS;
    return bloom_test_bit(bloom, h1) &&
           bloom_test_bit(bloom, h2) &&
           bloom_test_bit(bloom, h3);
}

void tmk_bloom_clear(tmk_bloom_t *bloom) {
    tmk_bloom_init(bloom);
}

float tmk_bloom_fill_ratio(const tmk_bloom_t *bloom) {
    if (!bloom) return 0.0f;
    uint32_t set = 0;
    for (uint32_t i = 0; i < TMK_BLOOM_WORDS; i++) {
        uint32_t v = bloom->bits[i];
        /* Brian Kernighan's popcount */
        while (v) {
            v &= v - 1;
            set++;
        }
    }
    return (float)set / (float)TMK_BLOOM_BITS;
}
