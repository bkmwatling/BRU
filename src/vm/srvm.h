#ifndef BRU_VM_SRVM_H
#define BRU_VM_SRVM_H

#include <stc/fatp/string_view.h>

#include "thread_managers/thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_srvm BruSRVM;

typedef struct {
    StcStringView *captures; /**< the recorded match's captures               */
    bru_byte_t    *bytes;    /**< the recorded match's output bytes           */

    bru_len_t ncaptures; /**< the number of captures                          */
    size_t    nbytes; /**< the number of bytes                                */
} BruSRVMMatch;

#if !defined(BRU_VM_SRVM_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_SRVM_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&     \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||  \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef BruSRVM      SRVM;
typedef BruSRVMMatch SRVMMatch;

#    define srvm_new      bru_srvm_new
#    define srvm_free     bru_srvm_free
#    define srvm_match    bru_srvm_match
#    define srvm_find     bru_srvm_find
#    define srvm_capture  bru_srvm_capture
#    define srvm_captures bru_srvm_captures
#    define srvm_matches  bru_srvm_matches
#endif /* BRU_VM_SRVM_ENABLE_SHORT_NAMES */

/* --- SRVM function prototypes --------------------------------------------- */

/**
 * Construct a Symbolic Regular Expression Virtual Machine with given thream
 * manager and program.
 *
 * @param[in] thread_manager the thread manager for the SRVM to use
 * @param[in] prog           the program for the SRVM to execute
 *
 * @return the constructed SRVM
 */
BruSRVM *bru_srvm_new(BruThreadManager *thread_manager, const BruProgram *prog);

/**
 * Free the memory allocated for the SRVM.
 *
 * @param[in] self the SRVM to free
 */
void bru_srvm_free(BruSRVM *self);

/**
 * Execute the SRVM against an input string.
 *
 * @param[in] self the SRVM to execute
 * @param[in] text the input string to match against
 *
 * @return truthy value if the SRVM matched against the input string; else 0
 */
BruSRVMMatch *bru_srvm_match(BruSRVM *self, const char *text);

/**
 * Free the memory allocated for the SRVM match object.
 *
 * @param[in] self the SRVM match object to free
 */
void bru_srvm_match_free(BruSRVMMatch *self);

/**
 * Find the next match of the regex of the SRVM inside the input string if
 * possible for partial matching.
 *
 * @param[in] self the SRVM to execute
 * @param[in] text the input string to find next match in
 *
 * @return truthy value if the SRVM found a match in the input string; else 0
 */
BruSRVMMatch *bru_srvm_find(BruSRVM *self, const char *text);

/**
 * Determine whether the regex represented by the program matches the input text
 * by using the given thread manager.
 *
 * Convenience function if the same program does not need to be matched again
 * and capture information is not needed.
 *
 * @param[in] thread_manager the thread manager to execute with
 * @param[in] prog           the program to executein
 * @param[in] text           the input string to match against
 *
 * @return truthy value if the program matched against the input string; else 0
 */
int bru_srvm_matches(BruThreadManager *thread_manager,
                     const BruProgram *prog,
                     const char       *text);

#endif /* BRU_VM_SRVM_H */
