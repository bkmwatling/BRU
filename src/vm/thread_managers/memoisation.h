#ifndef BRU_VM_THREAD_MANAGER_MEMOISATION_H
#define BRU_VM_THREAD_MANAGER_MEMOISATION_H

#include "thread_manager.h"

#if !defined(BRU_VM_THREAD_MANAGER_MEMOISATION_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_MEMOISATION_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                           \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                        \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define memoised_thread_manager_new bru_memoised_thread_manager_new
#endif /* BRU_VM_THREAD_MANAGER_MEMOISATION_ENABLE_SHORT_NAMES */

/**
 * Construct a thread manager that implements memoisation around an underlying
 * thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 *
 * @return the constructed memoised thread manager
 */
BruThreadManager *
bru_memoised_thread_manager_new(BruThreadManager *thread_manager);

#endif /* BRU_VM_THREAD_MANAGER_MEMOISATION_H */
