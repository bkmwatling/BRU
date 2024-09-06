#ifndef BRU_RE_WALKER_REGEX_TO_STRING_H
#define BRU_RE_WALKER_REGEX_TO_STRING_H

#include "walker.h"

#if !defined(BRU_RE_WALKER_REGEX_TO_STRING_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_WALKER_REGEX_TO_STRING_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&                       \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||                    \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define regex_to_string bru_regex_to_string
#endif /* BRU_RE_WALKER_REGEX_TO_STRING_ENABLE_SHORT_NAMES */

/**
 * Convert the given regex tree to a string.
 *
 * @param[in] re the regular expression parse tree
 */
char *bru_regex_to_string(BruRegexNode *re);

#endif /* BRU_RE_WALKER_REGEX_TO_STRING_H */
