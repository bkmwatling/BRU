#ifndef BRU_WALKER_IAR_THOMPSON_H
#define BRU_WALKER_IAR_THOMPSON_H

#include "../walker.h"

#if !defined(BRU_RE_WALKER_IAR_THOMPSON_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_WALKER_IAR_THOMPSON_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&                    \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||                 \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define infinite_ambiguity_removal_thompson \
        bru_infinite_ambiguity_removal_thompson
#endif /* BRU_RE_WALKER_IAR_THOMPSON_ENABLE_SHORT_NAMES */

/**
 * Memoise the given regex according to IAR(E).
 *
 * @param[in] r a pointer to the parsed regular expression
 */
void bru_infinite_ambiguity_removal_thompson(BruRegexNode **r);

#endif /* BRU_WALKER_IAR_THOMPSON_H */
