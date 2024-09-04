#ifndef BRU_WALKER_IN_DEGREE_THOMPSON_H
#define BRU_WALKER_IN_DEGREE_THOMPSON_H

#include "../walker.h"

#if !defined(BRU_RE_WALKER_IN_DEGREE_THOMPSON_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_WALKER_IN_DEGREE_THOMPSON_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&                          \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||                       \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define in_degree_thompson bru_in_degree_thompson
#endif /* BRU_RE_WALKER_IN_DEGREE_THOMPSON_ENABLE_SHORT_NAMES */

/**
 * Memoise the regular expression according to IN(E) for the Thompson
 * construction.
 *
 * @param[in] r a pointer to the parsed regular expression
 */
void bru_in_degree_thompson(BruRegexNode **r);

#endif /* BRU_WALKER_IN_DEGREE_THOMPSON_H */
