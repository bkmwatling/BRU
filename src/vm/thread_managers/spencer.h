#ifndef BRU_VM_THREAD_MANAGER_SPENCER_H
#define BRU_VM_THREAD_MANAGER_SPENCER_H

#include <stdio.h>

#include "../../types.h"
#include "thread_manager.h"

#if !defined(BRU_VM_THREAD_MANAGER_SPENCER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_SPENCER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                       \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                    \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define spencer_thread_manager_new bru_spencer_thread_manager_new
#endif /* BRU_VM_THREAD_MANAGER_SPENCER_ENABLE_SHORT_NAMES */

/* --- Spencer ThreadManager function prototypes ---------------------------- */

/**
 * Construct a thread manager that performs Spencer-style regex matching.
 *
 * @return the constructed Spencer-style thread manager
 */
BruThreadManager *bru_spencer_thread_manager_new(void);

#endif /* BRU_VM_THREAD_MANAGER_SPENCER_H */
