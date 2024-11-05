#ifndef BRU_VM_THREAD_MANAGER_SCHEDULER_SPENCER_H
#define BRU_VM_THREAD_MANAGER_SCHEDULER_SPENCER_H

#include <stdio.h>

#include "scheduler.h"

#if !defined(BRU_VM_THREAD_MANAGER_SCHEDULER_SPENCER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_SCHEDULER_SPENCER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                                 \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                              \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define spencer_scheduler_new bru_spencer_scheduler_new
#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_SPENCER_ENABLE_SHORT_NAMES */

/* --- Spencer Scheduler function prototypes ---------------------------- */

/**
 * Construct a scheduler that performs Spencer-style DFS/Stack thread
 * scheduling.
 *
 * @return the constructed Spencer-style scheduler
 */
BruScheduler *bru_spencer_scheduler_new(void);

#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_SPENCER_H */
