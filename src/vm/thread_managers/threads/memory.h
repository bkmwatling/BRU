#ifndef BRU_VM_THREAD_MANAGER_MEMORY_THREAD_H
#define BRU_VM_THREAD_MANAGER_MEMORY_THREAD_H

#include <stdio.h>

#include "thread.h"

#if !defined(BRU_VM_THREAD_MANAGER_MEMORY_THREAD_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_MEMORY_THREAD_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                             \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                          \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define memory_thread_new bru_memory_thread_new
#endif /* BRU_VM_THREAD_MANAGER_MEMORY_ENABLE_SHORT_NAMES */

/* --- Memory Thread function prototypes ---------------------------------- */

/**
 * Construct a thread for execution with memory.
 *
 * The thread will still need to be initialised (e.g. via bru_thread_init or
 * bru_thread_copy).
 *
 * @param[in] memory_len the length of the memory
 * @param[in] thread     the underlying thread
 *
 * @return the constructed BruThread
 */
BruThread *bru_memory_thread_new(bru_len_t memory_len, BruThread *thread);

#endif /* BRU_VM_THREAD_MANAGER_MEMORY_THREAD_H */
