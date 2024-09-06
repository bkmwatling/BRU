#ifndef BRU_RE_WALKER_IN_DEGREE_GLUSHKOV_H
#define BRU_RE_WALKER_IN_DEGREE_GLUSHKOV_H

#include "../walker.h"

#if !defined(BRU_RE_WALKER_IN_DEGREE_GLUSHKOV_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_WALKER_IN_DEGREE_GLUSHKOV_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&                          \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||                       \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define in_degree_glushkov bru_in_degree_glushkov
#endif /* BRU_RE_WALKER_IN_DEGREE_GLUSHKOV_ENABLE_SHORT_NAMES */

/**
 * Memoise the regular expression according to IN(E) for the Glushkov
 * construction.
 *
 * @param[in] r a pointer to the parsed regular expression
 */
void bru_in_degree_glushkov(BruRegexNode **r);

#endif /* BRU_RE_WALKER_IN_DEGREE_GLUSHKOV_H */
