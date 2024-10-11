#ifndef BRU_VM_THREAD_MANAGER_SCHEDULER_H
#define BRU_VM_THREAD_MANAGER_SCHEDULER_H

/**
 * The Scheduler interface specifies functions for manipulating execution order
 * of threads. It is used by different thread managers.
 */

#include "../thread_manager.h"

#define bru_scheduler_init(scheduler) ((scheduler)->init((scheduler)->impl))
#define bru_scheduler_schedule(scheduler, thread) \
    ((scheduler)->schedule((scheduler)->impl, (thread)))
#define bru_scheduler_schedule_in_order(scheduler, thread) \
    ((scheduler)->schedule_in_order((scheduler)->impl, (thread)))
#define bru_scheduler_has_next(scheduler) \
    ((scheduler)->has_next((scheduler)->impl))
#define bru_scheduler_next(scheduler) ((scheduler)->next((scheduler)->impl))
#define bru_scheduler_free(scheduler)         \
    do {                                      \
        (scheduler)->free((scheduler)->impl); \
        free((scheduler));                    \
    } while (0)

typedef struct {
    /**
     * Initialise the scheduler.
     */
    void (*init)(void *scheduler_impl);

    /**
     * Schedules a thread.
     *
     * The scheduling order is an implementation detail.
     *
     * Returns true/false values if scheduling succeeds/fails.
     */
    int (*schedule)(void *scheduler_impl, BruThread *thread);

    /**
     * Schedules a thread.
     *
     * The order in which the thread is scheduled must be such that consecutive
     * calls to schedule_in_order result in later threads being run after
     * earlier threads (i.e., scheduling order must be maintained).
     *
     * Returns true/false values if scheduling succeeds/fails.
     */
    int (*schedule_in_order)(void *scheduler_impl, BruThread *thread);

    /**
     * Check if the scheduler is empty.
     */
    int (*has_next)(const void *scheduler_impl);

    /**
     * Get the next thread for execution.
     *
     * Returns NULL if the scheduler is empty.
     */
    BruThread *(*next)(void *scheduler_impl);

    /**
     * Free the resources used by the scheduler.
     *
     * NOTE: Threads still held by the scheduler _must_ be free'd manually, it
     * is not handled by this function.
     */
    void (*free)(void *scheduler_impl);

    void *impl; /**< the underlying implementation of the scheduler           */
} BruScheduler;

#if !defined(BRU_VM_THREAD_MANAGER_SCHEDULER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_SCHEDULER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                         \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                      \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define scheduler_init              bru_scheduler_init
#    define scheduler_schedule          bru_scheduler_schedule
#    define scheduler_schedule_in_order bru_scheduler_schedule_in_order
#    define scheduler_has_next          bru_scheduler_has_next
#    define scheduler_next              bru_scheduler_next
#    define scheduler_free              bru_scheduler_free

typedef BruScheduler Scheduler;
#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_ENABLE_SHORT_NAMES */

#endif /* BRU_VM_THREAD_MANAGER_SCHEDULER_H */
