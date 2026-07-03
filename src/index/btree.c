/*
 * libtidemark — B-tree index operations
 *
 * Implements a logical B-tree over index nodes for efficient key-range
 * lookups.  Each node covers a key range; the B-tree maintains sorted
 * order by key_min for binary-search-based lookup.
 *
 * Split: when a node's child_count exceeds TMK_BTREE_ORDER, the node
 * is split into two at the median key.  The new node inherits the
 * upper half of the key range.
 *
 * Merge: when two adjacent nodes both have fewer than TMK_BTREE_MIN_KEYS
 * children, they are merged.  The surviving node absorbs the full key
 * range and child set of both.
 *
 * Rebalance: scans the tree for merge/split candidates and applies
 * the operations until the tree satisfies the B-tree invariant.
 */
#include "../internal/context.h"
#include "btree.h"
#include "../../include/tidemark/error.h"
#include <string.h>

int tmk_btree_init(tmk_btree_t *tree) {
    if (!tree) return TMK_ERR_SHORT_BUF;
    memset(tree, 0, sizeof(*tree));
    return TMK_OK;
}

/* Insertion sort by key_min to maintain sorted order */
static int sorted_insert(tmk_context_t *ctx, tmk_btree_t *tree,
                         uint16_t node_id) {
    tmk_index_node_t *node = tmk_find_index_node(ctx, node_id);
    if (!node || !node->active) return TMK_ERR_NOT_FOUND;

    uint16_t pos = tree->count;
    for (uint16_t i = 0; i < tree->count; i++) {
        tmk_index_node_t *existing = tmk_find_index_node(ctx, tree->node_ids[i]);
        if (existing && node->key_min < existing->key_min) {
            pos = i;
            break;
        }
    }

    /* Shift elements right */
    if (pos < tree->count) {
        memmove(&tree->node_ids[pos + 1], &tree->node_ids[pos],
                (tree->count - pos) * sizeof(uint16_t));
    }

    tree->node_ids[pos] = node_id;
    tree->count++;
    tree->total_keys += node->child_count;

    if (tree->count == 1) {
        tree->root_id = node_id;
    }

    return TMK_OK;
}

int tmk_btree_insert_node(tmk_context_t *ctx, tmk_btree_t *tree,
                          uint16_t node_id) {
    if (!tree) return TMK_ERR_SHORT_BUF;
    if (tree->count >= TMK_MAX_INDEX_NODES) return TMK_ERR_INDEX_FULL;

    /* Check for duplicate */
    for (uint16_t i = 0; i < tree->count; i++) {
        if (tree->node_ids[i] == node_id) return TMK_ERR_ALREADY_EXISTS;
    }

    return sorted_insert(ctx, tree, node_id);
}

int tmk_btree_remove_node(tmk_btree_t *tree, uint16_t node_id) {
    if (!tree) return TMK_ERR_SHORT_BUF;

    for (uint16_t i = 0; i < tree->count; i++) {
        if (tree->node_ids[i] == node_id) {
            if (i + 1 < tree->count) {
                memmove(&tree->node_ids[i], &tree->node_ids[i + 1],
                        (tree->count - i - 1) * sizeof(uint16_t));
            }
            tree->count--;
            return TMK_OK;
        }
    }

    return TMK_ERR_NOT_FOUND;
}

int tmk_btree_search(tmk_context_t *ctx, const tmk_btree_t *tree,
                     uint32_t key, uint16_t *out_node_id) {
    if (!tree || !out_node_id) return TMK_ERR_SHORT_BUF;
    if (tree->count == 0) return TMK_ERR_NOT_FOUND;

    /* Binary search for the node covering this key */
    int lo = 0, hi = (int)tree->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        tmk_index_node_t *node = tmk_find_index_node(ctx, tree->node_ids[mid]);
        if (!node) { hi = mid - 1; continue; }

        if (key < node->key_min) {
            hi = mid - 1;
        } else if (key > node->key_max) {
            lo = mid + 1;
        } else {
            /* Key falls within this node's range */
            *out_node_id = tree->node_ids[mid];
            return TMK_OK;
        }
    }

    return TMK_ERR_NOT_FOUND;
}

int tmk_btree_range_scan(tmk_context_t *ctx, const tmk_btree_t *tree,
                         uint32_t key_lo, uint32_t key_hi,
                         uint16_t *out_nodes, uint16_t max_out,
                         uint16_t *out_count) {
    if (!tree || !out_nodes || !out_count) return TMK_ERR_SHORT_BUF;
    *out_count = 0;

    for (uint16_t i = 0; i < tree->count && *out_count < max_out; i++) {
        tmk_index_node_t *node = tmk_find_index_node(ctx, tree->node_ids[i]);
        if (!node || !node->active) continue;

        /* Check overlap: node range intersects query range */
        if (node->key_max >= key_lo && node->key_min <= key_hi) {
            out_nodes[*out_count] = tree->node_ids[i];
            (*out_count)++;
        }

        /* Optimization: if node->key_min > key_hi, all subsequent nodes
         * will also be beyond the range (sorted order) */
        if (node->key_min > key_hi) break;
    }

    return TMK_OK;
}

int tmk_btree_split_node(tmk_context_t *ctx, tmk_btree_t *tree,
                         uint16_t node_id) {
    tmk_index_node_t *node = tmk_find_index_node(ctx, node_id);
    if (!node || !node->active) return TMK_ERR_NOT_FOUND;
    if (node->child_count <= TMK_BTREE_ORDER) return TMK_OK;

    /* Find a free index slot for the new node */
    int new_slot = -1;
    for (int i = 0; i < TMK_MAX_INDEX_NODES; i++) {
        if (!ctx->index_nodes[i].active) { new_slot = i; break; }
    }
    if (new_slot < 0) return TMK_ERR_INDEX_FULL;

    /* Calculate split point */
    uint32_t median = node->key_min + (node->key_max - node->key_min) / 2;

    /* Create new node for upper half */
    ctx->index_nodes[new_slot].id = (uint16_t)(new_slot + 1);
    ctx->index_nodes[new_slot].active = true;
    ctx->index_nodes[new_slot].segment_id = node->segment_id;
    ctx->index_nodes[new_slot].partition_id = node->partition_id;
    ctx->index_nodes[new_slot].generation = node->generation;
    ctx->index_nodes[new_slot].stored_capacity = node->stored_capacity;
    ctx->index_nodes[new_slot].segment_ref = node->segment_ref;
    ctx->index_nodes[new_slot].partition_ref = node->partition_ref;
    ctx->index_nodes[new_slot].cursor_ref = NULL;
    ctx->index_nodes[new_slot].tombstone = false;
    ctx->index_nodes[new_slot].create_epoch = node->create_epoch;
    ctx->index_nodes[new_slot].key_min = median + 1;
    ctx->index_nodes[new_slot].key_max = node->key_max;
    ctx->index_nodes[new_slot].depth = node->depth;
    ctx->index_nodes[new_slot].child_count = node->child_count / 2;

    /* Adjust original node to lower half */
    node->key_max = median;
    node->child_count -= ctx->index_nodes[new_slot].child_count;

    /* Insert new node into tree */
    tmk_btree_insert_node(ctx, tree, ctx->index_nodes[new_slot].id);

    return TMK_OK;
}

int tmk_btree_merge_nodes(tmk_context_t *ctx, tmk_btree_t *tree,
                          uint16_t node_a, uint16_t node_b) {
    tmk_index_node_t *a = tmk_find_index_node(ctx, node_a);
    tmk_index_node_t *b = tmk_find_index_node(ctx, node_b);
    if (!a || !b || !a->active || !b->active) return TMK_ERR_NOT_FOUND;

    /* Ensure a < b in key order */
    if (a->key_min > b->key_min) {
        tmk_index_node_t *tmp = a; a = b; b = tmp;
        uint16_t t = node_a; node_a = node_b; node_b = t;
    }

    /* Merge b into a */
    a->key_max = b->key_max;
    a->child_count += b->child_count;
    tree->total_keys -= b->child_count; /* will be re-added as merged */

    /* Remove b from tree */
    b->active = false;
    tmk_btree_remove_node(tree, node_b);

    return TMK_OK;
}

int tmk_btree_rebalance(tmk_context_t *ctx, tmk_btree_t *tree) {
    if (!tree || tree->count < 2) return TMK_OK;
    int ops = 0;

    /* Split overloaded nodes */
    for (uint16_t i = 0; i < tree->count; i++) {
        tmk_index_node_t *node = tmk_find_index_node(ctx, tree->node_ids[i]);
        if (node && node->child_count > TMK_BTREE_ORDER) {
            tmk_btree_split_node(ctx, tree, tree->node_ids[i]);
            ops++;
        }
    }

    /* Merge underloaded adjacent nodes */
    for (uint16_t i = 0; i + 1 < tree->count; i++) {
        tmk_index_node_t *a = tmk_find_index_node(ctx, tree->node_ids[i]);
        tmk_index_node_t *b = tmk_find_index_node(ctx, tree->node_ids[i + 1]);
        if (a && b &&
            a->child_count < TMK_BTREE_MIN_KEYS &&
            b->child_count < TMK_BTREE_MIN_KEYS) {
            tmk_btree_merge_nodes(ctx, tree, tree->node_ids[i],
                                  tree->node_ids[i + 1]);
            ops++;
            /* Re-scan from same position since array shifted */
            if (i > 0) i--;
        }
    }

    return ops;
}

bool tmk_btree_validate(tmk_context_t *ctx, const tmk_btree_t *tree) {
    if (!tree || tree->count == 0) return true;

    /* Check sorted order and no overlapping ranges */
    for (uint16_t i = 0; i + 1 < tree->count; i++) {
        tmk_index_node_t *a = tmk_find_index_node(ctx, tree->node_ids[i]);
        tmk_index_node_t *b = tmk_find_index_node(ctx, tree->node_ids[i + 1]);
        if (!a || !b) return false;

        /* key_max of a must be < key_min of b */
        if (a->key_max >= b->key_min) return false;

        /* Each node must have key_min <= key_max */
        if (a->key_min > a->key_max) return false;
    }

    /* Validate last node */
    tmk_index_node_t *last = tmk_find_index_node(ctx, tree->node_ids[tree->count - 1]);
    if (last && last->key_min > last->key_max) return false;

    return true;
}

int tmk_btree_depth(const tmk_btree_t *tree) {
    return tree ? (int)tree->depth : 0;
}

int tmk_btree_node_count(const tmk_btree_t *tree) {
    return tree ? (int)tree->count : 0;
}
