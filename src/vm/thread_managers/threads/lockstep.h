#ifndef BRU_VM_THREAD_MANAGER_LOCKSTEP_THREAD_H
#define BRU_VM_THREAD_MANAGER_LOCKSTEP_THREAD_H

#include <stdio.h>

#include "thread.h"

#if !defined(BRU_VM_THREAD_MANAGER_LOCKSTEP_THREAD_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_LOCKSTEP_THREAD_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                               \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                            \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define lockstep_thread_new bru_lockstep_thread_new
#endif /* BRU_VM_THREAD_MANAGER_LOCKSTEP_ENABLE_SHORT_NAMES */

/* --- Lockstep Thread function prototypes ---------------------------------- */

/**
 * Construct a thread for Lockstep execution.
 *
 * The thread will still need to be initialised (e.g. via bru_thread_init or
 * bru_thread_copy).
 *
 * @return the constructed BruThread
 */
BruThread *bru_lockstep_thread_new(void);

#endif /* BRU_VM_THREAD_MANAGER_LOCKSTEP_THREAD_H */
