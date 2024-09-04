#ifndef BRU_FA_CONSTRUCT_GLUSHKOV_H
#define BRU_FA_CONSTRUCT_GLUSHKOV_H

#include "../../vm/compiler.h"
#include "../smir.h"

#if !defined(BRU_FA_CONSTRUCT_GLUSHKOV_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_CONSTRUCT_GLUSHKOV_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&                   \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||                \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define glushkov_construct bru_glushkov_construct
#endif /* BRU_FA_CONSTRUCT_GLUSHKOV_ENABLE_SHORT_NAMES */

/**
 * Construct a state machine from a regex tree using the Glushkov construction.
 *
 * @param[in] re   the regex string and tree
 * @param[in] opts the compiler options for construction
 *
 * @return the Glushkov constructed state machine
 */
BruStateMachine *bru_glushkov_construct(BruRegex               re,
                                        const BruCompilerOpts *opts);

#endif /* BRU_FA_CONSTRUCT_GLUSHKOV_H */
