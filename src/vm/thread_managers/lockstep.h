#ifndef BRU_VM_THREAD_MANAGER_LOCKSTEP_H
#define BRU_VM_THREAD_MANAGER_LOCKSTEP_H

#include <stdio.h>

#include "../../types.h"
#include "thread_manager.h"

#if !defined(BRU_VM_THREAD_MANAGER_LOCKSTEP_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_LOCKSTEP_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                        \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                     \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define thompson_thread_manager_new bru_thompson_thread_manager_new
#endif /* BRU_VM_THREAD_MANAGER_LOCKSTEP_ENABLE_SHORT_NAMES */

/* --- Thompson ThreadManager function prototypes --------------------------- */

/**
 * Construct a thread manager that performs Thompson-style lockstep regex
 * matching.
 *
 * @return the constructed Thompson-style thread manager
 */
BruThreadManager *bru_thompson_thread_manager_new(void);

#endif /* BRU_VM_THREAD_MANAGER_LOCKSTEP_H */
