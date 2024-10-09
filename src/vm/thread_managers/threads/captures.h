#ifndef BRU_VM_THREAD_MANAGER_CAPTURES_THREAD_H
#define BRU_VM_THREAD_MANAGER_CAPTURES_THREAD_H

#include <stdio.h>

#include "thread.h"

#if !defined(BRU_VM_THREAD_MANAGER_CAPTURES_THREAD_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_CAPTURES_THREAD_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                               \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                            \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define captures_thread_new bru_captures_thread_new
#endif /* BRU_VM_THREAD_MANAGER_CAPTURES_ENABLE_SHORT_NAMES */

/* --- Captures Thread function prototypes ---------------------------------- */

/**
 * Construct a thread for execution with captures.
 *
 * The thread will still need to be initialised (e.g. via bru_thread_init or
 * bru_thread_copy).
 *
 * @param[in] ncaptures the number of captures
 * @param[in] thread    the underlying thread
 *
 * @return the constructed BruThread
 */
BruThread *bru_captures_thread_new(bru_len_t ncaptures, BruThread *thread);

#endif /* BRU_VM_THREAD_MANAGER_CAPTURES_THREAD_H */
