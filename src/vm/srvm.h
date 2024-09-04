#ifndef BRU_VM_SRVM_H
#define BRU_VM_SRVM_H

#include "../stc/fatp/string_view.h"

#include "thread_managers/thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_srvm BruSRVM;

#if !defined(BRU_VM_SRVM_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_SRVM_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&     \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||  \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define NO_MATCH          BRU_NO_MATCH
#    define MATCH             BRU_MATCH
#    define MATCHES_EXHAUSTED BRU_MATCHES_EXHAUSTED

typedef BruSRVM SRVM;

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
int bru_srvm_match(BruSRVM *self, const char *text);

/**
 * Find the next match of the regex of the SRVM inside the input string if
 * possible for partial matching.
 *
 * @param[in] self the SRVM to execute
 * @param[in] text the input string to find next match in
 *
 * @return truthy value if the SRVM found a match in the input string; else 0
 */
int bru_srvm_find(BruSRVM *self, const char *text);

/**
 * Get the string view into the input string of the capture at the given index
 * from the previous match.
 *
 * @param[in] self the SRVM to get the capture from
 * @param[in] idx  the index of the capture
 *
 * @return the string view of the capture
 */
StcStringView bru_srvm_capture(BruSRVM *self, bru_len_t idx);

/**
 * Get the string views of all the captures from the previous match of the SRVM.
 *
 * @param[in]  self      the SRVM to get the captures from
 * @param[out] ncaptures the number of captures in the returned array
 *
 * @return the array of capture string views from the SRVM
 */
StcStringView *bru_srvm_captures(BruSRVM *self, bru_len_t *ncaptures);

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
