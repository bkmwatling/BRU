#ifndef BRU_FA_TRANSFORM_FLATTEN_H
#define BRU_FA_TRANSFORM_FLATTEN_H

#include "../smir.h"

/**
 * XXX: ONLY TO BE USED ON THOMPSON-CONSTRUCTED STATE MACHINES,
 * BEFORE MEMOISATION
 */

#if !defined(BRU_FA_TRANSFORM_FLATTEN_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_TRANSFORM_FLATTEN_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&                  \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||               \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define transform_flatten bru_transform_flatten
#endif /* BRU_FA_TRANSFORM_FLATTEN_ENABLE_SHORT_NAMES */

/**
 * Flatten a state machine to have the properties that all states match a
 * character and all other actions are on transitions between states.
 *
 * NOTE: This algorithm also eliminates paths in the original state machine that
 * are equivalent to a higher priority path, where equivalence is determined by
 * the predicates on the path. The number of such eliminated paths is recorded
 * in the provided logfile.
 *
 * @param[in] sm      the state machine
 * @param[in] logfile file for logging
 *
 * @return the flattened state machine
 */
BruStateMachine *bru_transform_flatten(BruStateMachine *sm, FILE *logfile);

#endif /* BRU_FA_TRANSFORM_FLATTEN_H */
