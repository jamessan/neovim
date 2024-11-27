#include "nvim/marktree.h"

size_t marktree_check_node(MarkTree *b, MTNode *x, MTPos *last, bool *last_right,
                           const uint32_t *meta_node_ref);
bool marktree_check_intersections(MarkTree *b);
void mt_recurse_nodes(MTNode *x, PMap(ptr_t) *checked);
bool mt_recurse_nodes_compare(MTNode *x, PMap(ptr_t) *checked);
