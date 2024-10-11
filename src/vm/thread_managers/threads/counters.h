#ifndef BRU_VM_THREAD_MANAGER_COUNTERS_THREAD_H
#define BRU_VM_THREAD_MANAGER_COUNTERS_THREAD_H

#include <stdio.h>

#include "thread.h"

#if !defined(BRU_VM_THREAD_MANAGER_COUNTERS_THREAD_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_COUNTERS_THREAD_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                               \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                            \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define counters_thread_new bru_counters_thread_new
#endif /* BRU_VM_THREAD_MANAGER_COUNTERS_ENABLE_SHORT_NAMES */

/* --- Counters Thread function prototypes ---------------------------------- */

/**
 * Construct a thread for execution with counters.
 *
 * The thread will still need to be initialised (e.g. via bru_thread_init or
 * bru_thread_copy).
 *
 * @param[in] ncounters the number of counters
 * @param[in] thread    the underlying thread
 *
 * @return the constructed BruThread
 */
BruThread *bru_counters_thread_new(bru_len_t ncounters, BruThread *thread);

#endif /* BRU_VM_THREAD_MANAGER_COUNTERS_THREAD_H */
