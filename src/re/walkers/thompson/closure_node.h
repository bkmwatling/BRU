#ifndef BRU_WALKER_CLOSURE_NODE_THOMPSON_H
#define BRU_WALKER_CLOSURE_NODE_THOMPSON_H

#include "../walker.h"

#if !defined(BRU_RE_WALKER_CLOSURE_NODE_THOMPSON_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_WALKER_CLOSURE_NODE_THOMPSON_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&                             \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||                          \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define closure_node_thompson bru_closure_node_thompson
#endif /* BRU_RE_WALKER_CLOSURE_NODE_THOMPSON_ENABLE_SHORT_NAMES */

/**
 * Apply CN(E) memoisation strategy to the given regular expression.
 *
 * @param[in] r the parsed regular expression.
 */
void bru_closure_node_thompson(BruRegexNode **r);

#endif /* BRU_WALKER_CLOSURE_NODE_THOMPSON_H */
