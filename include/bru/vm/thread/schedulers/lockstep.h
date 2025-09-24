#ifndef BRU_VM_THREAD_SCHEDULER_LOCKSTEP_H
#define BRU_VM_THREAD_SCHEDULER_LOCKSTEP_H

#include <stdbool.h>

#include <bru/vm/thread/schedulers/scheduler.h>

#if !defined(BRU_VM_THREAD_SCHEDULER_LOCKSTEP_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_SCHEDULER_LOCKSTEP_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                          \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                       \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define lockstep_ts_new bru_lockstep_ts_new
#    define lockstep_ts_remove_low_priority_threads \
        bru_lockstep_ts_remove_low_priority_threads
#    define lockstep_ts_done_step bru_lockstep_ts_done_step
#endif /* BRU_VM_THREAD_SCHEDULER_LOCKSTEP_ENABLE_SHORT_NAMES */

/* --- Lockstep Thread Scheduler function prototypes ------------------------ */

/**
 * Construct a thread scheduler that performs lockstep thread scheduling.
 *
 * The thread manager is required since lockstep needs to inspect the program
 * counters for each thread to determine if it should be made to wait for other
 * threads or not (e.g. in the case of the CHAR/PRED instructions).
 *
 * @param[in] tm the thread manager using the thread scheduler
 *
 * @return the constructed lockstep thread scheduler
 */
BruThreadScheduler *bru_lockstep_ts_new(BruThreadManager *tm);

/**
 * Remove (and return) the low priority threads from the thread scheduler.
 *
 * 'Low priority' in this case refers to any threads in the currently executing
 * queue. The order of the threads in the returned StcVec is not necessarily
 * the prioritised order.
 *
 * The returned vec will need to be free'd appropriately.
 *
 * @param self[in] the lockstep thread scheduler
 *
 * @return a new StcVec containing the lower priority threads, or NULL if there
 *         aren't any.
 */
StcVec(BruThread *)
bru_lockstep_ts_remove_low_priority_threads(BruThreadScheduler *self);

/**
 * Check if the thread scheduler has performed a 'step'.
 *
 * A 'step' is the process of executing all threads until they reach either a
 * PRED or CHAR instruction. If this function returns a true value, then all
 * threads in the thread scheduler are pointing at CHAR/PRED instructions after
 * being executed by the VM at least once since the previous 'step'.
 *
 * @param self[in] the lockstep thread scheduler
 *
 * @returns true if a step has just been completed; else false
 */
bool bru_lockstep_ts_done_step(BruThreadScheduler *self);

#endif /* BRU_VM_THREAD_SCHEDULER_LOCKSTEP_H */
