#ifndef TMK_INTERNAL_BITSET_H
#define TMK_INTERNAL_BITSET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Fixed-size bit vector for tracking segment/partition occupancy.
 * Used by the partition registry for fast segment-presence checks
 * and by the retention enforcer for expiry tracking.
 */

#define TMK_BITSET_MAX_BITS 512
#define TMK_BITSET_WORDS ((TMK_BITSET_MAX_BITS + 31) / 32)

typedef struct {
    uint32_t words[TMK_BITSET_WORDS];
    uint32_t count;      /* number of set bits */
    uint32_t capacity;   /* configured capacity in bits */
} tmk_bitset_t;

void tmk_bitset_init(tmk_bitset_t *bs, uint32_t capacity);
void tmk_bitset_set(tmk_bitset_t *bs, uint32_t bit);
void tmk_bitset_clear(tmk_bitset_t *bs, uint32_t bit);
bool tmk_bitset_test(const tmk_bitset_t *bs, uint32_t bit);
void tmk_bitset_clear_all(tmk_bitset_t *bs);
uint32_t tmk_bitset_count(const tmk_bitset_t *bs);
int  tmk_bitset_first_set(const tmk_bitset_t *bs);
int  tmk_bitset_first_clear(const tmk_bitset_t *bs);
int  tmk_bitset_next_set(const tmk_bitset_t *bs, uint32_t after);
void tmk_bitset_or(tmk_bitset_t *dst, const tmk_bitset_t *src);
void tmk_bitset_and(tmk_bitset_t *dst, const tmk_bitset_t *src);
void tmk_bitset_xor(tmk_bitset_t *dst, const tmk_bitset_t *src);
void tmk_bitset_not(tmk_bitset_t *bs);
bool tmk_bitset_intersects(const tmk_bitset_t *a, const tmk_bitset_t *b);
bool tmk_bitset_subset(const tmk_bitset_t *subset, const tmk_bitset_t *superset);
bool tmk_bitset_empty(const tmk_bitset_t *bs);
float tmk_bitset_density(const tmk_bitset_t *bs);

#endif
