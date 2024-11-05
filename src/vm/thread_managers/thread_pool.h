#ifndef BRU_VM_THREAD_MANAGER_THREAD_POOL_H
#define BRU_VM_THREAD_MANAGER_THREAD_POOL_H

#include <stddef.h>
#include <stdio.h>

#include "thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

#if !defined(BRU_VM_THREAD_MANAGER_THREAD_POOL_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_THREAD_POOL_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                           \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                        \
          defined(BRU_ENABLE_SHORT_NAMES)))

#    define thread_pool_new bru_thread_pool_new
#endif /* BRU_VM_THREAD_MANAGER_THREAD_POOL_ENABLE_SHORT_NAMES */

/* --- API function prototypes ---------------------------------------------- */

/**
 * Create a new thread pool.
 *
 * A thread pool is used for managing thread memory.
 * Instead of killing a thread, a thread is added to the pool. When a new thread
 * is needed, a thread in the pool will be reused. If the pool is empty, then
 * NULL will be provided.
 *
 * @param[in] logfile the file for logging output
 *
 * @return the thread pool
 */
BruThreadManager *bru_thread_manager_with_pool_new(BruThreadManager *tm,
                                                   FILE             *logfile);

#endif /* BRU_VM_THREAD_MANAGER_THREAD_POOL_H */
