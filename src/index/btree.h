#ifndef TMK_INDEX_BTREE_H
#define TMK_INDEX_BTREE_H

#include "../../include/tidemark/types.h"

/*
 * B-tree-style index operations for segment key-range management.
 *
 * Index nodes form a logical B-tree where each node covers a
 * contiguous key range [key_min, key_max].  Splits and merges
 * maintain balanced coverage across partition boundaries.
 *
 * The B-tree ordering invariant: for any node N with children
 * C1..Cn, key_max(Ci) < key_min(Ci+1).
 */

#define TMK_BTREE_ORDER 4
#define TMK_BTREE_MIN_KEYS (TMK_BTREE_ORDER / 2)

typedef struct {
    uint16_t node_ids[TMK_MAX_INDEX_NODES];
    uint16_t count;
    uint32_t total_keys;
    uint16_t depth;
    uint16_t root_id;
} tmk_btree_t;

int  tmk_btree_init(tmk_btree_t *tree);
int  tmk_btree_insert_node(tmk_context_t *ctx, tmk_btree_t *tree,
                           uint16_t node_id);
int  tmk_btree_remove_node(tmk_btree_t *tree, uint16_t node_id);
int  tmk_btree_search(tmk_context_t *ctx, const tmk_btree_t *tree,
                      uint32_t key, uint16_t *out_node_id);
int  tmk_btree_range_scan(tmk_context_t *ctx, const tmk_btree_t *tree,
                          uint32_t key_lo, uint32_t key_hi,
                          uint16_t *out_nodes, uint16_t max_out,
                          uint16_t *out_count);
int  tmk_btree_split_node(tmk_context_t *ctx, tmk_btree_t *tree,
                          uint16_t node_id);
int  tmk_btree_merge_nodes(tmk_context_t *ctx, tmk_btree_t *tree,
                           uint16_t node_a, uint16_t node_b);
int  tmk_btree_rebalance(tmk_context_t *ctx, tmk_btree_t *tree);
bool tmk_btree_validate(tmk_context_t *ctx, const tmk_btree_t *tree);
int  tmk_btree_depth(const tmk_btree_t *tree);
int  tmk_btree_node_count(const tmk_btree_t *tree);

#endif
