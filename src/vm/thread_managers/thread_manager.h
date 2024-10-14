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
#include "../vtable.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define bru_thread_manager_init(manager, start_pc_in, start_sp_in) \
    bru_vt_call_procedure(manager, init, start_pc_in, start_sp_in)
#define bru_thread_manager_reset(manager) bru_vt_call_procedure(manager, reset)
#define bru_thread_manager_free(manager)      \
    do {                                      \
        bru_vt_call_procedure(manager, free); \
        bru_vt_release(manager);              \
    } while (0)
#define bru_thread_manager_done_exec(manager, is_done_out) \
    bru_vt_call_function(manager, is_done_out, done_exec)

// NOTE:
// alloc_thread and free_thread are not attached to each instance.
// This is because they essentially allocate and free the entrie block of thread
// memory according to the size of the thread the manager uses, which is always
// the size of the leaf instance.
#define bru_thread_manager_malloc_thread(manager)                    \
    ((BruThread *) malloc(                                           \
         (manager)->table[bru_vt_leaf_idx(manager)]->_thread_size) + \
     (manager)->table[bru_vt_leaf_idx(manager)]->_thread_size)
#define bru_thread_manager_alloc_thread(manager, thread_out) \
    bru_vt_call_function(manager, thread_out, alloc_thread)
#define bru_thread_manager_free_thread(manager, thread) \
    free((thread) - (manager)->table[bru_vt_leaf_idx(manager)]->_thread_size)

#define bru_thread_manager_init_thread(manager, thread_in, pc_in, sp_in) \
    bru_vt_call_procedure(manager, init_thread, thread_in, pc_in, sp_in)
#define bru_thread_manager_copy_thread(manager, thread_src, thread_dst) \
    bru_vt_call_procedure(manager, copy_thread, thread_src, thread_dst)
#define bru_thread_manager_schedule_thread(manager, thread_in) \
    bru_vt_call_procedure(manager, schedule_thread, thread_in)
#define bru_thread_manager_schedule_thread_in_order(manager, thread_in) \
    bru_vt_call_procedure(manager, schedule_thread_in_order, thread_in)
#define bru_thread_manager_next_thread(manager, thread_out) \
    bru_vt_call_function(manager, thread_out, next_thread)
#define bru_thread_manager_notify_thread_match(manager, thread_in) \
    bru_vt_call_procedure(manager, notify_thread_match, thread_in)
#define bru_thread_manager_clone_thread(manager, thread_out, thread_in) \
    bru_vt_call_function(manager, thread_out, clone_thread, thread_in)
#define bru_thread_manager_kill_thread(manager, thread_in) \
    bru_vt_call_procedure(manager, kill_thread, thread_in)
#define bru_thread_manager_check_thread_eq(manager, cmp_out, thread1_in, \
                                           thread2_in)                   \
    bru_vt_call_function(manager, cmp_out, check_thread_eq, thread1_in,  \
                         thread2_in)

#define bru_thread_manager_pc(manager, pc_out, thread_in) \
    bru_vt_call_function(manager, pc_out, pc, thread_in)
#define bru_thread_manager_set_pc(manager, thread_in, pc_in) \
    bru_vt_call_procedure(manager, set_pc, thread_in, pc_in)
#define bru_thread_manager_sp(manager, sp_out, thread_in) \
    bru_vt_call_function(manager, sp_out, sp, thread_in)
#define bru_thread_manager_inc_sp(manager, thread_in) \
    bru_vt_call_procedure(manager, inc_sp, thread_in)

#define bru_thread_manager_init_memoisation(manager, nmemo_in, text_len_in) \
    bru_vt_call_procedure(manager, init_memoisation, nmemo_in, text_len_in)
#define bru_thread_manager_memoise(manager, memoised_out, thread_in, idx_in) \
    bru_vt_call_function(manager, memoised_out, memoise, thread_in, idx_in)
#define bru_thread_manager_counter(manager, counter_out, thread_in, idx_in) \
    bru_vt_call_function(manager, counter_out, counter, thread_in, idx_in)
#define bru_thread_manager_set_counter(manager, thread_in, idx_in, val_in) \
    bru_vt_call_procedure(manager, set_counter, thread_in, idx_in, val_in)
#define bru_thread_manager_inc_counter(manager, thread_in, idx_in) \
    bru_vt_call_procedure(manager, inc_counter, thread_in, idx_in)
#define bru_thread_manager_memory(manager, memory_out, thread_in, idx_in) \
    bru_vt_call_function(manager, memory_out, memory, thread_in, idx_in)
#define bru_thread_manager_set_memory(manager, thread_in, idx_in, val_in, \
                                      size_in)                            \
    bru_vt_call_procedure(manager, set_memory, thread_in, idx_in, val_in, \
                          size_in)
#define bru_thread_manager_captures(manager, captures_out, thread_in, \
                                    ncaptures_in)                     \
    bru_vt_call_function(manager, captures_out, captures, thread_in,  \
                         ncaptures_in)
#define bru_thread_manager_set_capture(manager, thread_in, idx_in) \
    bru_vt_call_procedure(manager, set_capture, thread_in, idx_in)

#define BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(manager_interface, prefix)    \
    do {                                                                    \
        (manager_interface)->init      = prefix##_thread_manager_init;      \
        (manager_interface)->reset     = prefix##_thread_manager_reset;     \
        (manager_interface)->free      = prefix##_thread_manager_free;      \
        (manager_interface)->done_exec = prefix##_thread_manager_done_exec; \
                                                                            \
        (manager_interface)->alloc_thread =                                 \
            prefix##_thread_manager_alloc_thread;                           \
        (manager_interface)->init_thread =                                  \
            prefix##_thread_manager_init_thread;                            \
        (manager_interface)->copy_thread =                                  \
            prefix##_thread_manager_copy_thread;                            \
        (manager_interface)->clone_thread =                                 \
            prefix##_thread_manager_clone_thread;                           \
        (manager_interface)->kill_thread =                                  \
            prefix##_thread_manager_kill_thread;                            \
        (manager_interface)->check_thread_eq =                              \
            prefix##_thread_manager_check_thread_eq;                        \
        (manager_interface)->schedule_thread =                              \
            prefix##_thread_manager_schedule_thread;                        \
        (manager_interface)->schedule_thread_in_order =                     \
            prefix##_thread_manager_schedule_thread_in_order;               \
        (manager_interface)->next_thread =                                  \
            prefix##_thread_manager_next_thread;                            \
        (manager_interface)->notify_thread_match =                          \
            prefix##_thread_manager_notify_thread_match;                    \
                                                                            \
        (manager_interface)->pc     = prefix##_thread_manager_pc;           \
        (manager_interface)->set_pc = prefix##_thread_manager_set_pc;       \
        (manager_interface)->sp     = prefix##_thread_manager_sp;           \
        (manager_interface)->inc_sp = prefix##_thread_manager_inc_sp;       \
    } while (0)

#define BRU_THREAD_MANAGER_SET_NOOP_FUNCS(manager_interface)                  \
    do {                                                                      \
        (manager_interface)->init_memoisation =                               \
            bru_thread_manager_init_memoisation_noop;                         \
        (manager_interface)->memoise = bru_thread_manager_memoise_noop;       \
        (manager_interface)->counter = bru_thread_manager_counter_noop;       \
        (manager_interface)->set_counter =                                    \
            bru_thread_manager_set_counter_noop;                              \
        (manager_interface)->inc_counter =                                    \
            bru_thread_manager_inc_counter_noop;                              \
        (manager_interface)->memory     = bru_thread_manager_memory_noop;     \
        (manager_interface)->set_memory = bru_thread_manager_set_memory_noop; \
        (manager_interface)->bytes      = bru_thread_manager_bytes_noop;      \
        (manager_interface)->write_byte = bru_thread_manager_write_byte_noop; \
        (manager_interface)->captures   = bru_thread_manager_captures_noop;   \
        (manager_interface)->set_capture =                                    \
            bru_thread_manager_set_capture_noop;                              \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef bru_byte_t BruThread; /**< BruThread is a collection of bytes */

typedef BruVTable_of(struct bru_thread_manager_interface) BruThreadManager;

typedef struct bru_thread_manager_interface {
    void (*init)(BruThreadManager *self,
                 const bru_byte_t *start_pc,
                 const char       *start_sp);
    void (*reset)(BruThreadManager *self);
    void (*free)(BruThreadManager *self); /**< free the thread manager     */
    int (*done_exec)(BruThreadManager *self);

    // below functions manipulate thread execution
    BruThread *(*alloc_thread)(BruThreadManager *self);
    void (*init_thread)(BruThreadManager *self,
                        BruThread        *thread,
                        const bru_byte_t *pc,
                        const char       *sp);
    void (*copy_thread)(BruThreadManager *self,
                        const BruThread  *src,
                        BruThread        *dst);
    BruThread *(*clone_thread)(BruThreadManager *self, const BruThread *thread);
    void (*kill_thread)(BruThreadManager *self, BruThread *thread);

    /**< return 0 if equal, non-zero otherwise */
    int (*check_thread_eq)(BruThreadManager *self,
                           const BruThread  *t1,
                           const BruThread  *t2);
    void (*schedule_thread)(BruThreadManager *self, BruThread *thread);
    void (*schedule_thread_in_order)(BruThreadManager *self, BruThread *thread);
    BruThread *(*next_thread)(BruThreadManager *self);
    void (*notify_thread_match)(BruThreadManager *self, BruThread *thread);

    // functions that manipulate thread memory
    const bru_byte_t *(*pc)(BruThreadManager *self, const BruThread *thread);
    void (*set_pc)(BruThreadManager *self,
                   BruThread        *thread,
                   const bru_byte_t *pc);
    const char *(*sp)(BruThreadManager *self, const BruThread *thread);
    void (*inc_sp)(BruThreadManager *self, BruThread *thread);

    // memoisation
    void (*init_memoisation)(BruThreadManager *self,
                             size_t            nmemo_insts,
                             const char       *text);
    int (*memoise)(BruThreadManager *self, BruThread *thread, bru_len_t idx);

    // counters
    bru_cntr_t (*counter)(BruThreadManager *self,
                          const BruThread  *thread,
                          bru_len_t         idx);
    void (*set_counter)(BruThreadManager *self,
                        BruThread        *thread,
                        bru_len_t         idx,
                        bru_cntr_t        val);
    void (*inc_counter)(BruThreadManager *self,
                        BruThread        *thread,
                        bru_len_t         idx);

    // arbitrary memory
    void *(*memory)(BruThreadManager *self,
                    const BruThread  *thread,
                    bru_len_t         idx);
    void (*set_memory)(BruThreadManager *self,
                       BruThread        *thread,
                       bru_len_t         idx,
                       const void       *val,
                       size_t            size);

    // arbitrary writing bytes
    void (*write_byte)(BruThreadManager *self,
                       BruThread        *thread,
                       bru_byte_t        byte);
    bru_byte_t *(*bytes)(BruThreadManager *self,
                         BruThread        *thread,
                         size_t           *len);

    // captures
    const char *const *(*captures)(BruThreadManager *self,
                                   const BruThread  *thread,
                                   bru_len_t        *ncaptures);
    void (*set_capture)(BruThreadManager *self,
                        BruThread        *thread,
                        bru_len_t         idx);

    size_t _thread_size; /**< size of the thread used by this manager         */
    VTABLE_FIELDS;
} BruThreadManagerInterface;

#define BRU_THREAD_FROM_INSTANCE(instance, thread) \
    ((thread) - (instance)->_thread_size)

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

typedef BruThread                 Thread;
typedef BruThreadManager          ThreadManager;
typedef BruThreadManagerInterface ThreadManagerInterface;

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

/* --- Thread manager interface function prototypes ------------------------- */

/**
 * Create a new interface for a thread manager.
 *
 * All interface functions are set to NULL.
 *
 * @param[in] impl  the implementing object
 * @param[in] tsize the size of the thread used by the thread manager
 */
BruThreadManagerInterface *bru_thread_manager_interface_new(void  *impl,
                                                            size_t tsize);

/**
 * Free the thread manager interface.
 *
 * @param[in] tmi the thread manager interface
 */
void bru_thread_manager_interface_free(BruThreadManagerInterface *tmi);

/* --- Thread manager NO-OP function prototypes ----------------------------- */

// the below functions can be used as placeholders for interface functions where
// nothing should happen. Sensical return values are used -- NULL for pointers,
// truthy values for memoisation, and 0 for counter values.

void bru_thread_manager_init_memoisation_noop(BruThreadManager *tm,
                                              size_t            nmemo_insts,
                                              const char       *text);

int bru_thread_manager_memoise_noop(BruThreadManager *tm,
                                    BruThread        *thread,
                                    bru_len_t         idx);

bru_cntr_t bru_thread_manager_counter_noop(BruThreadManager *tm,
                                           const BruThread  *thread,
                                           bru_len_t         idx);

void bru_thread_manager_set_counter_noop(BruThreadManager *tm,
                                         BruThread        *thread,
                                         bru_len_t         idx,
                                         bru_cntr_t        val);

void bru_thread_manager_inc_counter_noop(BruThreadManager *tm,
                                         BruThread        *thread,
                                         bru_len_t         idx);

void *bru_thread_manager_memory_noop(BruThreadManager *tm,
                                     const BruThread  *thread,
                                     bru_len_t         idx);

void bru_thread_manager_set_memory_noop(BruThreadManager *tm,
                                        BruThread        *thread,
                                        bru_len_t         idx,
                                        const void       *val,
                                        size_t            size);

void bru_thread_manager_write_byte_noop(BruThreadManager *self,
                                        BruThread        *thread,
                                        bru_byte_t        byte);

bru_byte_t *bru_thread_manager_bytes_noop(BruThreadManager *self,
                                          BruThread        *thread,
                                          size_t           *len);

const char *const *bru_thread_manager_captures_noop(BruThreadManager *tm,
                                                    const BruThread  *thread,
                                                    bru_len_t *ncaptures);

void bru_thread_manager_set_capture_noop(BruThreadManager *tm,
                                         BruThread        *thread,
                                         bru_len_t         idx);

#endif /* BRU_VM_THREAD_MANAGER_H */
