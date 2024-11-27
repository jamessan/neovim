#include "marktree_test.h"

#include <assert.h>

#define T MT_BRANCH_FACTOR
#define ptr s->i_ptr
#define meta s->i_meta

size_t marktree_check_node(MarkTree *b, MTNode *x, MTPos *last, bool *last_right,
                           const uint32_t *meta_node_ref)
{
  assert(x->n <= 2 * T - 1);
  // TODO(bfredl): too strict if checking "in repair" post-delete tree.
  assert(x->n >= (x != b->root ? T - 1 : 0));
  size_t n_keys = (size_t)x->n;

  for (int i = 0; i < x->n; i++) {
    if (x->level) {
      n_keys += marktree_check_node(b, x->ptr[i], last, last_right, x->meta[i]);
    } else {
      *last = (MTPos) { 0, 0 };
    }
    if (i > 0) {
      mt_unrelative(x->key[i - 1].pos, last);
    }
    assert(mt_pos_leq(*last, x->key[i].pos));
    if (last->row == x->key[i].pos.row && last->col == x->key[i].pos.col) {
      assert(!*last_right || mt_right(x->key[i]));
    }
    *last_right = mt_right(x->key[i]);
    assert(x->key[i].pos.col >= 0);
    assert(pmap_get(uint64_t)(b->id2node, mt_lookup_key(x->key[i])) == x);
  }

  if (x->level) {
    n_keys += marktree_check_node(b, x->ptr[x->n], last, last_right, x->meta[x->n]);
    mt_unrelative(x->key[x->n - 1].pos, last);

    for (int i = 0; i < x->n + 1; i++) {
      assert(x->ptr[i]->parent == x);
      assert(x->ptr[i]->p_idx == i);
      assert(x->ptr[i]->level == x->level - 1);
      // PARANOIA: check no double node ref
      for (int j = 0; j < i; j++) {
        assert(x->ptr[i] != x->ptr[j]);
      }
    }
  } else if (x->n > 0) {
    *last = x->key[x->n - 1].pos;
  }

  uint32_t meta_node[4];
  mt_meta_describe_node(meta_node, x);
  for (int m = 0; m < kMTMetaCount; m++) {
    assert(meta_node_ref[m] == meta_node[m]);
  }

  return n_keys;
}

bool marktree_check_intersections(MarkTree *b)
{
  if (!b->root) {
    return true;
  }
  PMap(ptr_t) checked = MAP_INIT;

  // 1. move x->intersect to checked[x] and reinit x->intersect
  mt_recurse_nodes(b->root, &checked);

  // 2. iterate over all marks. for each START mark of a pair,
  // intersect the nodes between the pair
  MarkTreeIter itr[1];
  marktree_itr_first(b, itr);
  while (true) {
    MTKey mark = marktree_itr_current(itr);
    if (mark.pos.row < 0) {
      break;
    }

    if (mt_start(mark)) {
      MarkTreeIter start_itr[1];
      MarkTreeIter end_itr[1];
      uint64_t end_id = mt_lookup_id(mark.ns, mark.id, true);
      MTKey k = marktree_lookup(b, end_id, end_itr);
      if (k.pos.row >= 0) {
        *start_itr = *itr;
        marktree_intersect_pair(b, mt_lookup_key(mark), start_itr, end_itr, false);
      }
    }

    marktree_itr_next(b, itr);
  }

  // 3. for each node check if the recreated intersection
  // matches the old checked[x] intersection.
  bool status = mt_recurse_nodes_compare(b->root, &checked);

  uint64_t *val;
  map_foreach_value(&checked, val, {
    xfree(val);
  });
  map_destroy(ptr_t, &checked);

  return status;
}

void mt_recurse_nodes(MTNode *x, PMap(ptr_t) *checked)
{
  if (kv_size(x->intersect)) {
    kvi_push(x->intersect, (uint64_t)-1);  // sentinel
    uint64_t *val;
    if (x->intersect.items == x->intersect.init_array) {
      val = xmemdup(x->intersect.items, x->intersect.size * sizeof(*x->intersect.items));
    } else {
      val = x->intersect.items;
    }
    pmap_put(ptr_t)(checked, x, val);
    kvi_init(x->intersect);
  }

  if (x->level) {
    for (int i = 0; i < x->n + 1; i++) {
      mt_recurse_nodes(x->ptr[i], checked);
    }
  }
}

bool mt_recurse_nodes_compare(MTNode *x, PMap(ptr_t) *checked)
{
  uint64_t *ref = pmap_get(ptr_t)(checked, x);
  if (ref != NULL) {
    for (size_t i = 0;; i++) {
      if (ref[i] == (uint64_t)-1) {
        if (i != kv_size(x->intersect)) {
          return false;
        }

        break;
      } else {
        if (kv_size(x->intersect) <= i || ref[i] != kv_A(x->intersect, i)) {
          return false;
        }
      }
    }
  } else {
    if (kv_size(x->intersect)) {
      return false;
    }
  }

  if (x->level) {
    for (int i = 0; i < x->n + 1; i++) {
      if (!mt_recurse_nodes_compare(x->ptr[i], checked)) {
        return false;
      }
    }
  }

  return true;
}
