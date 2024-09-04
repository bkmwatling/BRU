#ifndef BRU_VM_THREAD_MANAGER_H
#define BRU_VM_THREAD_MANAGER_H

/**
 * The thread manager represents an interface for thread manipulation during
 * VM execution.
 *
 * In general, a thread manager should be able to create/kill threads, edit
 * thread memory, and provide a way for iterating over threads.
 *
 * The latter is generally thought to happen through the use of a scheduler.
 * Hence, we define both an interface for scheduling and managing threads here.
 * Each thread manager should have a scheduler it works with as well.
 *
 * The benefits of the below interfaces is it becomes easy to build a thread
 * manager (scheduler) from individual thread managers (schedulers). For
 * example, we do not have to define a memoisation variant for every thread
 * manager. Instead, we define a memoisation thread manager that wraps any
 * underlying thread manager.
 */

#include "../../types.h"
#include "../program.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define bru_thread_manager_init(manager, start_pc, start_sp) \
    (manager)->init((manager)->impl, (start_pc), (start_sp))
#define bru_thread_manager_reset(manager) (manager)->reset((manager)->impl)
#define bru_thread_manager_free(manager)  \
    do {                                  \
        (manager)->free((manager)->impl); \
        free((manager));                  \
    } while (0)
#define bru_thread_manager_done_exec(manager) \
    (manager)->done_exec((manager)->impl)

#define bru_thread_manager_schedule_thread(manager, thread) \
    (manager)->schedule_thread((manager)->impl, (thread))
#define bru_thread_manager_schedule_thread_in_order(manager, thread) \
    (manager)->schedule_thread_in_order((manager)->impl, (thread))
#define bru_thread_manager_next_thread(manager) \
    (manager)->next_thread((manager)->impl)
#define bru_thread_manager_notify_thread_match(manager, thread) \
    (manager)->notify_thread_match((manager)->impl, (thread))
#define bru_thread_manager_clone_thread(manager, thread) \
    (manager)->clone_thread((manager)->impl, (thread))
#define bru_thread_manager_kill_thread(manager, thread) \
    (manager)->kill_thread((manager)->impl, (thread))

#define bru_thread_manager_pc(manager, thread) \
    (manager)->pc((manager)->impl, (thread))
#define bru_thread_manager_set_pc(manager, thread, pc) \
    (manager)->set_pc((manager)->impl, (thread), (pc))
#define bru_thread_manager_sp(manager, thread) \
    (manager)->sp((manager)->impl, (thread))
#define bru_thread_manager_inc_sp(manager, thread) \
    (manager)->inc_sp((manager)->impl, (thread))

#define bru_thread_manager_init_memoisation(manager, nmemo, text_len) \
    (manager)->init_memoisation((manager)->impl, (nmemo), (text_len));
#define bru_thread_manager_memoise(manager, thread, idx) \
    (manager)->memoise((manager)->impl, (thread), (idx))
#define bru_thread_manager_counter(manager, thread, idx) \
    (manager)->counter((manager)->impl, (thread), (idx))
#define bru_thread_manager_set_counter(manager, thread, idx, val) \
    (manager)->set_counter((manager)->impl, (thread), (idx), (val))
#define bru_thread_manager_inc_counter(manager, thread, idx) \
    (manager)->inc_counter((manager)->impl, (thread), (idx))
#define bru_thread_manager_memory(manager, thread, idx) \
    (manager)->memory((manager)->impl, (thread), (idx))
#define bru_thread_manager_set_memory(manager, thread, idx, val, size) \
    (manager)->set_memory((manager)->impl, (thread), (idx), (val), (size))
#define bru_thread_manager_captures(manager, thread, ncaptures) \
    (manager)->captures((manager)->impl, (thread), (ncaptures))
#define bru_thread_manager_set_capture(manager, thread, idx) \
    (manager)->set_capture((manager)->impl, (thread), (idx))

#define BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(manager, prefix)                \
    do {                                                                      \
        (manager)->init      = prefix##_thread_manager_init;                  \
        (manager)->reset     = prefix##_thread_manager_reset;                 \
        (manager)->free      = prefix##_thread_manager_free;                  \
        (manager)->done_exec = prefix##_thread_manager_done_exec;             \
                                                                              \
        (manager)->schedule_thread = prefix##_thread_manager_schedule_thread; \
        (manager)->schedule_thread_in_order =                                 \
            prefix##_thread_manager_schedule_thread_in_order;                 \
        (manager)->next_thread = prefix##_thread_manager_next_thread;         \
        (manager)->notify_thread_match =                                      \
            prefix##_thread_manager_notify_thread_match;                      \
        (manager)->clone_thread = prefix##_thread_manager_clone_thread;       \
        (manager)->kill_thread  = prefix##_thread_manager_kill_thread;        \
                                                                              \
        (manager)->pc     = prefix##_thread_pc;                               \
        (manager)->set_pc = prefix##_thread_set_pc;                           \
        (manager)->sp     = prefix##_thread_sp;                               \
        (manager)->inc_sp = prefix##_thread_inc_sp;                           \
    } while (0)

#define BRU_THREAD_MANAGER_SET_ALL_FUNCS(manager, prefix)       \
    do {                                                        \
        BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(manager, prefix); \
                                                                \
        (manager)->init_memoisation =                           \
            prefix##_thread_manager_init_memoisation;           \
        (manager)->memoise     = prefix##_thread_memoise;       \
        (manager)->counter     = prefix##_thread_counter;       \
        (manager)->set_counter = prefix##_thread_set_counter;   \
        (manager)->inc_counter = prefix##_thread_inc_counter;   \
        (manager)->memory      = prefix##_thread_memory;        \
        (manager)->set_memory  = prefix##_thread_set_memory;    \
        (manager)->captures    = prefix##_thread_captures;      \
        (manager)->set_capture = prefix##_thread_set_capture;   \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

// the basic thread represent a state in the VM:
// some pointer into the instruction stream, and a pointer into the matching
// string.
//
// Any extensions of this (captures, counters, etc) can happen, but the datatype
// must adhere to this spec.
typedef struct {
    const bru_byte_t *pc; /**< the program counter of the thread              */
    const char       *sp; /**< the string pointer of the thread               */
} BruThread;

typedef struct thread_manager {
    void (*init)(void             *thread_manager_impl,
                 const bru_byte_t *start_pc,
                 const char       *start_sp);
    void (*reset)(void *thread_manager_impl);
    void (*free)(void *thread_manager_impl); /**< free the thread manager     */
    int  (*done_exec)(void *thread_manager_impl);

    // below functions manipulate thread execution
    void       (*schedule_thread)(void *thread_manager_impl, BruThread *thread);
    void       (*schedule_thread_in_order)(void      *thread_manager_impl,
                                     BruThread *thread);
    BruThread *(*next_thread)(void *thread_manager_impl);
    void (*notify_thread_match)(void *thread_manager_impl, BruThread *thread);
    BruThread *(*clone_thread)(void            *thread_manager_impl,
                               const BruThread *thread);
    void       (*kill_thread)(void *thread_manager_impl, BruThread *thread);

    // functions that manipulate thread memory
    const bru_byte_t *(*pc)(void *thread_manager_impl, const BruThread *thread);
    void              (*set_pc)(void             *thread_manager_impl,
                   BruThread        *thread,
                   const bru_byte_t *pc);
    const char       *(*sp)(void *thread_manager_impl, const BruThread *thread);
    void              (*inc_sp)(void *thread_manager_impl, BruThread *thread);

    // non-required interface functions
    void (*init_memoisation)(void       *thread_manager_impl,
                             size_t      nmemo_insts,
                             const char *text);
    int (*memoise)(void *thread_manager_impl, BruThread *thread, bru_len_t idx);
    bru_cntr_t         (*counter)(void            *thread_manager_impl,
                          const BruThread *thread,
                          bru_len_t        idx);
    void               (*set_counter)(void      *thread_manager_impl,
                        BruThread *thread,
                        bru_len_t  idx,
                        bru_cntr_t val);
    void               (*inc_counter)(void      *thread_manager_impl,
                        BruThread *thread,
                        bru_len_t  idx);
    void              *(*memory)(void            *thread_manager_impl,
                    const BruThread *thread,
                    bru_len_t        idx);
    void               (*set_memory)(void       *thread_manager_impl,
                       BruThread  *thread,
                       bru_len_t   idx,
                       const void *val,
                       size_t      size);
    const char *const *(*captures)(void            *thread_manager_impl,
                                   const BruThread *thread,
                                   bru_len_t       *ncaptures);
    void               (*set_capture)(void      *thread_manager_impl,
                        BruThread *thread,
                        bru_len_t  idx);

    void *impl; /**< the underlying implementation                            */
} BruThreadManager;

#if !defined(BRU_VM_THREAD_MANAGER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&               \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||            \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define thread_manager_init      bru_thread_manager_init
#    define thread_manager_reset     bru_thread_manager_reset
#    define thread_manager_free      bru_thread_manager_free
#    define thread_manager_done_exec bru_thread_manager_done_exec

#    define thread_manager_schedule_thread bru_thread_manager_schedule_thread
#    define thread_manager_schedule_thread_in_order \
        bru_thread_manager_schedule_thread_in_order
#    define thread_manager_next_thread bru_thread_manager_next_thread
#    define thread_manager_notify_thread_match \
        bru_thread_manager_notify_thread_match
#    define thread_manager_clone_thread bru_thread_manager_clone_thread
#    define thread_manager_kill_thread  bru_thread_manager_kill_thread

#    define thread_manager_pc     bru_thread_manager_pc
#    define thread_manager_set_pc bru_thread_manager_set_pc
#    define thread_manager_sp     bru_thread_manager_sp
#    define thread_manager_inc_sp bru_thread_manager_inc_sp

#    define thread_manager_init_memoisation bru_thread_manager_init_memoisation
#    define thread_manager_memoise          bru_thread_manager_memoise
#    define thread_manager_counter          bru_thread_manager_counter
#    define thread_manager_set_counter      bru_thread_manager_set_counter
#    define thread_manager_inc_counter      bru_thread_manager_inc_counter
#    define thread_manager_memory           bru_thread_manager_memory
#    define thread_manager_set_memory       bru_thread_manager_set_memory
#    define thread_manager_captures         bru_thread_manager_captures
#    define thread_manager_set_capture      bru_thread_manager_set_capture

#    define THREAD_MANAGER_SET_REQUIRED_FUNCS \
        BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS
#    define THREAD_MANAGER_SET_ALL_FUNCS BRU_THREAD_MANAGER_SET_ALL_FUNCS

typedef BruThread        Thread;
typedef BruThreadManager ThreadManager;

#    define thread_manager_init_memoisation_noop \
        bru_thread_manager_init_memoisation_noop
#    define thread_manager_memoise_noop     bru_thread_manager_memoise_noop
#    define thread_manager_counter_noop     bru_thread_manager_counter_noop
#    define thread_manager_set_counter_noop bru_thread_manager_set_counter_noop
#    define thread_manager_inc_counter_noop bru_thread_manager_inc_counter_noop
#    define thread_manager_memory_noop      bru_thread_manager_memory_noop
#    define thread_manager_set_memory_noop  bru_thread_manager_set_memory_noop
#    define thread_manager_captures_noop    bru_thread_manager_captures_noop
#    define thread_manager_set_capture_noop bru_thread_manager_set_capture_noop
#endif /* BRU_VM_THREAD_MANAGER_ENABLE_SHORT_NAMES */

/* --- Thread manager NO-OP function prototypes ----------------------------- */

// the below functions can be used as placeholders for interface functions where
// nothing should happen. Sensical return values are used -- NULL for pointers,
// truthy values for memoisation, and 0 for counter values.

void bru_thread_manager_init_memoisation_noop(void       *thread_manager_impl,
                                              size_t      nmemo_insts,
                                              const char *text);

int bru_thread_manager_memoise_noop(void      *thread_manager_impl,
                                    BruThread *thread,
                                    bru_len_t  idx);

bru_cntr_t bru_thread_manager_counter_noop(void            *thread_manager_impl,
                                           const BruThread *thread,
                                           bru_len_t        idx);

void bru_thread_manager_set_counter_noop(void      *thread_manager_impl,
                                         BruThread *thread,
                                         bru_len_t  idx,
                                         bru_cntr_t val);

void bru_thread_manager_inc_counter_noop(void      *thread_manager_impl,
                                         BruThread *thread,
                                         bru_len_t  idx);

void *bru_thread_manager_memory_noop(void            *thread_manager_impl,
                                     const BruThread *thread,
                                     bru_len_t        idx);

void bru_thread_manager_set_memory_noop(void       *thread_manager_impl,
                                        BruThread  *thread,
                                        bru_len_t   idx,
                                        const void *val,
                                        size_t      size);

const char *const *bru_thread_manager_captures_noop(void *thread_manager_impl,
                                                    const BruThread *thread,
                                                    bru_len_t       *ncaptures);

void bru_thread_manager_set_capture_noop(void      *thread_manager_impl,
                                         BruThread *thread,
                                         bru_len_t  idx);

#endif /* BRU_VM_THREAD_MANAGER_H */
