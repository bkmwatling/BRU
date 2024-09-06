#ifndef BRU_VM_THREAD_MANAGER_THREAD_POOL_H
#define BRU_VM_THREAD_MANAGER_THREAD_POOL_H

#include <stddef.h>
#include <stdio.h>

#include "thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_thread_pool BruThreadPool;

#if !defined(BRU_VM_THREAD_MANAGER_THREAD_POOL_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_THREAD_POOL_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                           \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                        \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef BruThreadPool ThreadPool;

#    define thread_pool_new        bru_thread_pool_new
#    define thread_pool_free       bru_thread_pool_free
#    define thread_pool_get_thread bru_thread_pool_get_thread
#    define thread_pool_add_thread bru_thread_pool_add_thread
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
BruThreadPool *bru_thread_pool_new(FILE *logfile);

/**
 * Free the resources used by the thread pool.
 *
 * Reports in the logfile provided at creaion the total number of threads
 * freed.
 *
 * @param[in] self        the thread pool
 * @param[in] thread_free a function for freeing the resources of a thread
 */
void bru_thread_pool_free(BruThreadPool *self,
                          void           (*thread_free)(BruThread *t));

/**
 * Get a new thread from the pool.
 *
 * The values of the returned thread's members should be considered garbage, and
 * need to be initialised appropriately.
 *
 * @param[in] self the thread pool
 *
 * @return a thread from the pool, or NULL if the pool is empty
 */
BruThread *bru_thread_pool_get_thread(BruThreadPool *self);

/**
 * Add a thread to the pool.
 *
 * Threads in the pool should be considered 'dead', and should not be used after
 * calling this function, unless retrieved using `thread_pool_get_thread`.
 *
 * @param[in] self   the thread pool
 * @param[in] thread the thread
 */
void bru_thread_pool_add_thread(BruThreadPool *self, BruThread *thread);

#endif /* BRU_VM_THREAD_MANAGER_THREAD_POOL_H */
