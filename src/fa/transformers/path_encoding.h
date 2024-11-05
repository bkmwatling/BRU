#ifndef BRU_FA_TRANSFORM_PATH_ENCODING_H
#define BRU_FA_TRANSFORM_PATH_ENCODING_H

#include "../../vm/compiler.h"
#include "../smir.h"

#if !defined(BRU_FA_TRANSFORM_PATH_ENCODING_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_TRANSFORM_PATH_ENCODING_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&                        \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||                     \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define transform_path_encode bru_transform_path_encode
#endif /* BRU_FA_TRANSFORM_PATH_ENCODING_ENABLE_SHORT_NAMES */

/**
 * Prepends path encoding actions to multiple transitions leaving a state.
 *
 * Path encoding actions tell the compiler to insert WRITE instructions which
 * will record the order the transitions are taken during SRVM execution.
 *
 * NOTE: This transform does not create a new state machine.
 *
 * @param[in] sm      the state machine
 *
 * @return the original state machine
 */
BruStateMachine *bru_transform_path_encode(BruStateMachine *sm);

#endif /* BRU_FA_TRANSFORM_PATH_ENCODING_H */
