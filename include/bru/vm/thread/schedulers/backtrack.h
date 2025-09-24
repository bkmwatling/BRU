#ifndef BRU_VM_THREAD_SCHEDULER_BACKTRACK_H
#define BRU_VM_THREAD_SCHEDULER_BACKTRACK_H

#include <bru/vm/thread/schedulers/scheduler.h>

#if !defined(BRU_VM_THREAD_SCHEDULER_BACKTRACK_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_SCHEDULER_BACKTRACK_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                           \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                        \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define backtrack_ts_new bru_backtrack_ts_new
#endif /* BRU_VM_THREAD_SCHEDULER_BACKTRACK_ENABLE_SHORT_NAMES */

/* --- Backtrack Thread Scheduler function prototypes ----------------------- */

/**
 * Construct a thread scheduler that performs Spencer-style DFS/stack
 * backtracking thread scheduling.
 *
 * @return the constructed backtracking thread scheduler
 */
BruThreadScheduler *bru_backtrack_ts_new(void);

#endif /* BRU_VM_THREAD_SCHEDULER_BACKTRACK_H */
