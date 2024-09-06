#ifndef BRU_VM_THREAD_MANAGER_SCHEDULER_H
#define BRU_VM_THREAD_MANAGER_SCHEDULER_H

/**
 * The Scheduler interface specifies functions for manipulating execution order
 * of threads in the SRVM.
 *
 * It has since been deprecated in favour of using the thread manager interface.
 */

#include "thread_manager.h"

#define bru_scheduler_schedule(scheduler, thread) \
    ((scheduler)->schedule((scheduler)->impl, (thread)))
#define bru_scheduler_schedule_in_order(scheduler, thread) \
    ((scheduler)->schedule_in_order((scheduler)->impl, (thread)))
#define bru_scheduler_has_next(scheduler) \
    ((scheduler)->has_next((scheduler)->impl))
#define bru_scheduler_next(scheduler)  ((scheduler)->next((scheduler)->impl))
#define bru_scheduler_clear(scheduler) ((scheduler)->clear((scheduler)->impl))
#define bru_scheduler_notify_match(scheduler) \
    ((scheduler)->notify_match((scheduler)->impl))
#define bru_scheduler_free(scheduler)         \
    do {                                      \
        (scheduler)->free((scheduler)->impl); \
        free((scheduler));                    \
    } while (0)

typedef struct {
    void       (*schedule)(void *scheduler_impl, BruThread *thread);
    void       (*schedule_in_order)(void *scheduler_impl, BruThread *thread);
    int        (*has_next)(const void *scheduler_impl);
    BruThread *(*next)(void *scheduler_impl);
    void       (*clear)(void *scheduler_impl);
    void       (*free)(void *scheduler_impl);

    void *impl; /**< the underlying implementation of the scheduler           */
} BruScheduler;

#if !defined(BRU_VM_THREAD_MANAGER_SCHEDULER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_SCHEDULER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                         \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                      \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define scheduler_schedule          bru_scheduler_schedule
#    define scheduler_schedule_in_order bru_scheduler_schedule_in_order
#    define scheduler_has_next          bru_scheduler_has_next
#    define scheduler_next              bru_scheduler_next
#    define scheduler_clear             bru_scheduler_clear
#    define scheduler_notify_match      bru_scheduler_notify_match
#    define scheduler_free              bru_scheduler_free

typedef BruScheduler Scheduler;
#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_ENABLE_SHORT_NAMES */

#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_H */
