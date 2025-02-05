#ifndef BRU_FA_CONSTRUCT_THOMPSON_H
#define BRU_FA_CONSTRUCT_THOMPSON_H

#include <bru/fa/constructions/opts.h>
#include <bru/fa/smir.h>

#if !defined(BRU_FA_CONSTRUCT_THOMPSON_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_CONSTRUCT_THOMPSON_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&                   \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||                \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define thompson_construct bru_thompson_construct
#endif /* BRU_FA_CONSTRUCT_THOMPSON_ENABLE_SHORT_NAMES */

/**
 * Construct a state machine from a regex tree using the Thompson construction.
 *
 * @param[in] re   the regex string and tree
 * @param[in] opts the construction options
 *
 * @return the Thompson constructed state machine
 */
BruStateMachine *bru_thompson_construct(BruRegex re, BruConstructionOpts opts);

#endif /* BRU_FA_CONSTRUCT_THOMPSON_H */
