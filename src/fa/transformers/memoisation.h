#ifndef BRU_FA_TRANSFORM_MEMOISATION_H
#define BRU_FA_TRANSFORM_MEMOISATION_H

#include "../../vm/compiler.h"
#include "../smir.h"

#if !defined(BRU_FA_TRANSFORM_MEMOISATION_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_TRANSFORM_MEMOISATION_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&                      \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||                   \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define transform_memoise bru_transform_memoise
#endif /* BRU_FA_TRANSFORM_MEMOISATION_ENABLE_SHORT_NAMES */

/**
 * Prepend memoisation actions to states in the given state machine according to
 * the provided memoisation scheme.
 *
 * NOTE: This transform does not create a new state machine.
 *
 * MS_IN: Memoise states with more than 1 incoming transition.
 * MS_CN: Memoise all states that are targets of backedges.
 * MS_IAR: Not currently supported.
 *
 * @param[in] sm      the state machine
 * @param[in] memo    the memoisation scheme
 * @param[in] logfile the file for logging output
 *
 * @return the original state machine
 */
BruStateMachine *
bru_transform_memoise(BruStateMachine *sm, BruMemoScheme memo, FILE *logfile);

#endif /* BRU_FA_TRANSFORM_MEMOISATION_H */
