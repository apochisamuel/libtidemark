#ifndef TMK_INTERNAL_HASH_H
#define TMK_INTERNAL_HASH_H

#include <stdint.h>
#include <stddef.h>

/*
 * Hash utilities for fast ID lookups.
 * Uses FNV-1a for general hashing and a custom mixing function
 * for 32-bit integer keys (session/channel/partition IDs).
 */

uint32_t tmk_hash_fnv1a(const uint8_t *data, size_t len);
uint32_t tmk_hash_u32(uint32_t key);
uint32_t tmk_hash_u16(uint16_t key);
uint64_t tmk_hash_combine(uint64_t h1, uint64_t h2);
uint32_t tmk_hash_bytes(const void *data, size_t len, uint32_t seed);

/* Bloom filter for tombstone existence checks */
#define TMK_BLOOM_BITS 256
#define TMK_BLOOM_WORDS (TMK_BLOOM_BITS / 32)

typedef struct {
    uint32_t bits[TMK_BLOOM_WORDS];
    uint32_t count;
} tmk_bloom_t;

void     tmk_bloom_init(tmk_bloom_t *bloom);
void     tmk_bloom_insert(tmk_bloom_t *bloom, uint32_t key);
int      tmk_bloom_check(const tmk_bloom_t *bloom, uint32_t key);
void     tmk_bloom_clear(tmk_bloom_t *bloom);
float    tmk_bloom_fill_ratio(const tmk_bloom_t *bloom);

#endif
