#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "program.h"
#include "types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    const byte *(*pc)(const void *thread);
    void (*set_pc)(void *thread, const byte *pc);
    const char *(*sp)(const void *thread);
    void (*try_inc_sp)(void *thread);
    const char *const *(*captures)(const void *thread, len_t *ncaptures);
    void (*set_capture)(void *thread, len_t idx);
    cntr_t (*counter)(const void *thread, len_t idx);
    void (*set_counter)(void *thread, len_t idx, cntr_t val);
    void (*inc_counter)(void *thread, len_t idx);
    void *(*memory)(const void *thread, len_t idx);
    void (*set_memory)(void *thread, len_t idx, const void *val, size_t size);
    void *(*clone)(const void *thread);
    void (*free)(void *thread);
} ThreadManager;

typedef struct {
    void (*init)(void *scheduler_impl, const char *text);
    void (*schedule)(void *scheduler_impl, void *thread);
    void (*schedule_in_order)(void *scheduler_impl, void *thread);
    int (*has_next)(const void *scheduler_impl);
    void *(*next)(void *scheduler_impl);
    void (*reset)(void *scheduler_impl);
    void (*notify_match)(void *scheduler_impl);
    void (*kill)(void *scheduler_impl, void *thread);
    void *(*clone_with)(const void *scheduler_impl, void *thread);
    const Program *(*program)(const void *scheduler_impl);
    void (*free)(void *scheduler_impl);

    void *impl; /*<< pointer to actual implementation of the scheduler */
} Scheduler;

/* --- Scheduler macro functions -------------------------------------------- */

#define scheduler_init(scheduler, text) \
    ((scheduler)->init((scheduler)->impl, (text)))
#define scheduler_schedule(scheduler, thread) \
    ((scheduler)->schedule((scheduler)->impl, (thread)))
#define scheduler_schedule_in_order(scheduler, thread) \
    ((scheduler)->schedule_in_order((scheduler)->impl, (thread)))
#define scheduler_has_next(scheduler) ((scheduler)->has_next((scheduler)->impl))
#define scheduler_next(scheduler)     ((scheduler)->next((scheduler)->impl))
#define scheduler_reset(scheduler)    ((scheduler)->reset((scheduler)->impl))
#define scheduler_notify_match(scheduler) \
    ((scheduler)->notify_match((scheduler)->impl))
#define scheduler_kill(scheduler, thread) \
    ((scheduler)->kill((scheduler)->impl, (thread)))
#define scheduler_copy_with(scheduler_dst, scheduler_src, thread) \
    memcpy((scheduler_dst), (scheduler_src), sizeof(Scheduler));  \
    (scheduler_dst)->impl =                                       \
        ((scheduler_src)->clone_with((scheduler_src)->impl, (thread)))
#define scheduler_program(scheduler) ((scheduler)->program((scheduler)->impl))
#define scheduler_free(scheduler)         \
    (scheduler)->free((scheduler)->impl); \
    free((scheduler))

/* --- Convenient wrapper macros -------------------------------------------- */

#define THREAD_MANAGER_DEFINE_WRAPPERS(prefix, thread_type)                    \
    static const byte *prefix##_pc_wrapper(const void *thread)                 \
    {                                                                          \
        return prefix##_pc((const thread_type *) thread);                      \
    }                                                                          \
                                                                               \
    static void prefix##_set_pc_wrapper(void *thread, const byte *pc)          \
    {                                                                          \
        prefix##_set_pc((thread_type *) thread, pc);                           \
    }                                                                          \
                                                                               \
    static const char *prefix##_sp_wrapper(const void *thread)                 \
    {                                                                          \
        return prefix##_sp((const thread_type *) thread);                      \
    }                                                                          \
                                                                               \
    static void prefix##_try_inc_sp_wrapper(void *thread)                      \
    {                                                                          \
        prefix##_try_inc_sp((thread_type *) thread);                           \
    }                                                                          \
                                                                               \
    static const char *const *prefix##_captures_wrapper(const void *thread,    \
                                                        len_t      *ncaptures) \
    {                                                                          \
        return prefix##_captures((const thread_type *) thread, ncaptures);     \
    }                                                                          \
                                                                               \
    static void prefix##_set_capture_wrapper(void *thread, len_t idx)          \
    {                                                                          \
        prefix##_set_capture((thread_type *) thread, idx);                     \
    }                                                                          \
                                                                               \
    static cntr_t prefix##_counter_wrapper(const void *thread, len_t idx)      \
    {                                                                          \
        return prefix##_counter((const thread_type *) thread, idx);            \
    }                                                                          \
                                                                               \
    static void prefix##_set_counter_wrapper(void *thread, len_t idx,          \
                                             cntr_t val)                       \
    {                                                                          \
        prefix##_set_counter((thread_type *) thread, idx, val);                \
    }                                                                          \
                                                                               \
    static void prefix##_inc_counter_wrapper(void *thread, len_t idx)          \
    {                                                                          \
        prefix##_inc_counter((thread_type *) thread, idx);                     \
    }                                                                          \
                                                                               \
    static void *prefix##_memory_wrapper(const void *thread, len_t idx)        \
    {                                                                          \
        return prefix##_memory((const thread_type *) thread, idx);             \
    }                                                                          \
                                                                               \
    static void prefix##_set_memory_wrapper(void *thread, len_t idx,           \
                                            const void *val, size_t size)      \
    {                                                                          \
        prefix##_set_memory((thread_type *) thread, idx, val, size);           \
    }                                                                          \
                                                                               \
    static void *prefix##_clone_wrapper(const void *thread)                    \
    {                                                                          \
        return prefix##_clone((const thread_type *) thread);                   \
    }                                                                          \
                                                                               \
    static void prefix##_free_wrapper(void *thread)                            \
    {                                                                          \
        prefix##_free((thread_type *) thread);                                 \
    }

#define THREAD_MANAGER_DEFINE_CONSTRUCTOR_FUNCTION(func_name, prefix) \
    ThreadManager *func_name(void)                                    \
    {                                                                 \
        ThreadManager *manager = malloc(sizeof(ThreadManager));       \
                                                                      \
        manager->pc          = prefix##_pc_wrapper;                   \
        manager->set_pc      = prefix##_set_pc_wrapper;               \
        manager->sp          = prefix##_sp_wrapper;                   \
        manager->try_inc_sp  = prefix##_try_inc_sp_wrapper;           \
        manager->captures    = prefix##_captures_wrapper;             \
        manager->set_capture = prefix##_set_capture_wrapper;          \
        manager->counter     = prefix##_counter_wrapper;              \
        manager->set_counter = prefix##_set_counter_wrapper;          \
        manager->inc_counter = prefix##_inc_counter_wrapper;          \
        manager->memory      = prefix##_memory_wrapper;               \
        manager->set_memory  = prefix##_set_memory_wrapper;           \
        manager->clone       = prefix##_clone_wrapper;                \
        manager->free        = prefix##_free_wrapper;                 \
                                                                      \
        return manager;                                               \
    }

#define SCHEDULER_DEFINE_WRAPPERS(prefix, scheduler_type, thread_type)         \
    static void prefix##_init_wrapper(void *scheduler_impl, const char *text)  \
    {                                                                          \
        prefix##_init((scheduler_type *) scheduler_impl, text);                \
    }                                                                          \
                                                                               \
    static void prefix##_schedule_wrapper(void *scheduler_impl, void *thread)  \
    {                                                                          \
        prefix##_schedule((scheduler_type *) scheduler_impl,                   \
                          (thread_type *) thread);                             \
    }                                                                          \
                                                                               \
    static void prefix##_schedule_in_order_wrapper(void *scheduler_impl,       \
                                                   void *thread)               \
    {                                                                          \
        prefix##_schedule_in_order((scheduler_type *) scheduler_impl,          \
                                   (thread_type *) thread);                    \
    }                                                                          \
                                                                               \
    static int prefix##_has_next_wrapper(const void *scheduler_impl)           \
    {                                                                          \
        return prefix##_has_next((const scheduler_type *) scheduler_impl);     \
    }                                                                          \
                                                                               \
    static void *prefix##_next_wrapper(void *scheduler_impl)                   \
    {                                                                          \
        return prefix##_next((scheduler_type *) scheduler_impl);               \
    }                                                                          \
                                                                               \
    static void prefix##_reset_wrapper(void *scheduler_impl)                   \
    {                                                                          \
        prefix##_reset((scheduler_type *) scheduler_impl);                     \
    }                                                                          \
                                                                               \
    static void prefix##_notify_match_wrapper(void *scheduler_impl)            \
    {                                                                          \
        prefix##_notify_match((scheduler_type *) scheduler_impl);              \
    }                                                                          \
                                                                               \
    static void prefix##_kill_wrapper(void *scheduler_impl, void *thread)      \
    {                                                                          \
        prefix##_kill((scheduler_type *) scheduler_impl,                       \
                      (thread_type *) thread);                                 \
    }                                                                          \
                                                                               \
    static void *prefix##_clone_with_wrapper(const void *scheduler_impl,       \
                                             void       *thread)               \
    {                                                                          \
        return prefix##_clone_with((const scheduler_type *) scheduler_impl,    \
                                   (thread_type *) thread);                    \
    }                                                                          \
                                                                               \
    static const Program *prefix##_program_wrapper(const void *scheduler_impl) \
    {                                                                          \
        return prefix##_program((const scheduler_type *) scheduler_impl);      \
    }                                                                          \
                                                                               \
    static void prefix##_free_wrapper(void *scheduler_impl)                    \
    {                                                                          \
        prefix##_free((scheduler_type *) scheduler_impl);                      \
    }                                                                          \
                                                                               \
    static void scheduler_free_noop(void *scheduler_impl)                      \
    {                                                                          \
        (void) scheduler_impl;                                                 \
    }                                                                          \
                                                                               \
    Scheduler *prefix##_as_scheduler(scheduler_type *scheduler_impl)           \
    {                                                                          \
        Scheduler *s = malloc(sizeof(Scheduler));                              \
                                                                               \
        s->init              = prefix##_init_wrapper;                          \
        s->schedule          = prefix##_schedule_wrapper;                      \
        s->schedule_in_order = prefix##_schedule_in_order_wrapper;             \
        s->has_next          = prefix##_has_next_wrapper;                      \
        s->next              = prefix##_next_wrapper;                          \
        s->reset             = prefix##_reset_wrapper;                         \
        s->notify_match      = prefix##_notify_match_wrapper;                  \
        s->kill              = prefix##_kill_wrapper;                          \
        s->clone_with        = prefix##_clone_with_wrapper;                    \
        s->program           = prefix##_program_wrapper;                       \
        s->free              = scheduler_free_noop;                            \
        s->impl              = scheduler_impl;                                 \
                                                                               \
        return s;                                                              \
    }                                                                          \
                                                                               \
    Scheduler *prefix##_to_scheduler(scheduler_type *scheduler_impl)           \
    {                                                                          \
        Scheduler *s = malloc(sizeof(Scheduler));                              \
                                                                               \
        s->init              = prefix##_init_wrapper;                          \
        s->schedule          = prefix##_schedule_wrapper;                      \
        s->schedule_in_order = prefix##_schedule_in_order_wrapper;             \
        s->has_next          = prefix##_has_next_wrapper;                      \
        s->next              = prefix##_next_wrapper;                          \
        s->reset             = prefix##_reset_wrapper;                         \
        s->notify_match      = prefix##_notify_match_wrapper;                  \
        s->kill              = prefix##_kill_wrapper;                          \
        s->clone_with        = prefix##_clone_with_wrapper;                    \
        s->program           = prefix##_program_wrapper;                       \
        s->free              = prefix##_free_wrapper;                          \
        s->impl              = scheduler_impl;                                 \
                                                                               \
        return s;                                                              \
    }

#endif /* SCHEDULER_H */
