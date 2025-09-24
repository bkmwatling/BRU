#ifndef BRU_RE_WALKER_MEMOISATION_H
#define BRU_RE_WALKER_MEMOISATION_H

// header aggregator for memoisation schemes

#include <bru/re/walkers/closure_node.h>
#include <bru/re/walkers/in_degree.h>
#include <bru/re/walkers/infinite_ambiguity_removal.h>

typedef void bru_memoise_f(BruRegexNode **r);

#if !defined(BRU_RE_WALKER_MEMOISATION_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_WALKER_MEMOISATION_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&                   \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||                \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef bru_memoise_f memoise_f;
#endif /* BRU_RE_WALKER_MEMOISATION_ENABLE_SHORT_NAMES */

#endif /* BRU_RE_WALKER_MEMOISATION_H */
