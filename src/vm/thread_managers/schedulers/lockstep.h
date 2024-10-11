#ifndef BRU_VM_THREAD_MANAGER_SCHEDULER_LOCKSTEP_H
#define BRU_VM_THREAD_MANAGER_SCHEDULER_LOCKSTEP_H

#include <stdio.h>

#include "scheduler.h"

#if !defined(BRU_VM_THREAD_MANAGER_SCHEDULER_LOCKSTEP_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_SCHEDULER_LOCKSTEP_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                                  \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                               \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define lockstep_scheduler_new bru_lockstep_scheduler_new
#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_LOCKSTEP_ENABLE_SHORT_NAMES */

/* --- lockstep Scheduler function prototypes ---------------------------- */

/**
 * Construct a scheduler that performs lockstep thread scheduling.
 *
 * The thread manager is required since lockstep needs to inspect the program
 * counters for each thread to determine if it should be made to wait for other
 * threads or not (e.g. in the case of the CHAR/PRED instructions).
 *
 * @param[in] tm the thread manager using the scheduler
 *
 * @return the constructed lockstep scheduler
 */
BruScheduler *bru_lockstep_scheduler_new(BruThreadManager *tm);

/**
 * Remove (and return) the low priority threads from the scheduler.
 *
 * 'Low priority' in this case refers to any threads in the currently executing
 * queue. The order of the threads in the returned stc_vec is not necessarily
 * the prioritised order.
 *
 * The returned vec will need to be free'd appropriately.
 *
 * @param self[in] the lockstep scheduler
 *
 * @return a new stc_vec containing the lower priority threads, or NULL if there
 *         aren't any.
 */
BruThread **
bru_lockstep_scheduler_remove_low_priority_threads(BruScheduler *self);

/**
 * Check if the scheduler has performed a 'step'.
 *
 * A 'step' is the process of executing all threads until they reach either a
 * PRED or CHAR instruction. If this function returns a true value, then all
 * threads in the scheduler are pointing at CHAR/PRED instructions after being
 * executed by the VM at least once since the previous 'step'.
 *
 * @param self[in] the Lockstep scheduler
 *
 * @returns TRUE if a step has just been completed, else FALSE
 */
int bru_lockstep_scheduler_done_step(BruScheduler *self);

#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_LOCKSTEP_H */
