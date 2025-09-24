#ifndef BRU_VM_THREAD_SCHEDULER_H
#define BRU_VM_THREAD_SCHEDULER_H

/**
 * The ThreadScheduler interface specifies functions for manipulating execution
 * order of threads. It is used by different thread managers.
 */

#include <stdbool.h>

#include <bru/vm/thread/managers/manager.h>

#define bru_ts_init(ts)             ((ts)->init((ts)->impl))
#define bru_ts_schedule(ts, thread) ((ts)->schedule((ts)->impl, (thread)))
#define bru_ts_schedule_in_order(ts, thread) \
    ((ts)->schedule_in_order((ts)->impl, (thread)))
#define bru_ts_has_next(ts) ((ts)->has_next((ts)->impl))
#define bru_ts_next(ts)     ((ts)->next((ts)->impl))
#define bru_ts_free(ts)         \
    do {                        \
        (ts)->free((ts)->impl); \
        free((ts));             \
    } while (0)

typedef struct {
    /**
     * Initialise the thread scheduler.
     */
    void (*init)(void *ts_impl);

    /**
     * Schedules a thread.
     *
     * The scheduling order is an implementation detail.
     *
     * Returns true/false values if scheduling succeeds/fails.
     */
    bool (*schedule)(void *ts_impl, BruThread *thread);

    /**
     * Schedules a thread.
     *
     * The order in which the thread is scheduled must be such that consecutive
     * calls to schedule_in_order result in later threads being run after
     * earlier threads (i.e., scheduling order must be maintained).
     *
     * Returns true/false values if scheduling succeeds/fails.
     */
    bool (*schedule_in_order)(void *ts_impl, BruThread *thread);

    /**
     * Check if the thread scheduler is empty.
     */
    bool (*has_next)(const void *ts_impl);

    /**
     * Get the next thread for execution.
     *
     * Returns NULL if the thread scheduler is empty.
     */
    BruThread *(*next)(void *ts_impl);

    /**
     * Free the resources used by the thread scheduler.
     *
     * NOTE: Threads still held by the thread scheduler _must_ be free'd
     * manually, it is not handled by this function.
     */
    void (*free)(void *ts_impl);

    void *impl; /**< the underlying implementation of the thread scheduler    */
} BruThreadScheduler;

#if !defined(BRU_VM_THREAD_SCHEDULER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_SCHEDULER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                 \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||              \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define ts_init              bru_ts_init
#    define ts_schedule          bru_ts_schedule
#    define ts_schedule_in_order bru_ts_schedule_in_order
#    define ts_has_next          bru_ts_has_next
#    define ts_next              bru_ts_next
#    define ts_free              bru_ts_free

typedef BruThreadScheduler ThreadScheduler;
#endif /* BRU_VM_THREAD_SCHEDULER_ENABLE_SHORT_NAMES */

#endif /* BRU_VM_THREAD_SCHEDULER_H */
