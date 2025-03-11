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

#include <bru/types.h>
#include <bru/vm/program.h>
#include <bru/vm/vtable.h>

/* --- Preprocessor directives ---------------------------------------------- */

#define bru_thread_manager_init(manager, start_pc, start_sp) \
    bru_vt_call_procedure(manager, init, start_pc, start_sp)
#define bru_thread_manager_reset(manager) bru_vt_call_procedure(manager, reset)
#define bru_thread_manager_kill(manager)      \
    do {                                      \
        bru_vt_call_procedure(manager, kill); \
        bru_vt_release(manager);              \
    } while (0)
#define bru_thread_manager_done_exec(manager) \
    bru_vt_call_function(manager, done_exec)
#define bru_thread_manager_get_match(manager) \
    bru_vt_call_function(manager, get_match)

/**
 * NOTE:
 * below macro used internally by thread manager implementations only.
 * Use the bru_thread_manager_kill macro defined above to kill a thread manager
 * and deallocate all of its memory.
 */
#define _bru_thread_manager_free(manager)             \
    do {                                              \
        bru_vt_call_procedure(manager, free);         \
        while (!stc_vec_is_empty((manager)->table))   \
            bru_thread_manager_interface_free(        \
                stc_vec_pop_back(&(manager)->table)); \
    } while (0)

/**
 * NOTE:
 * alloc_thread and free_thread are not attached to each instance.
 * This is because they essentially allocate and free the entire block of thread
 * memory according to the size of the thread the manager uses, which is always
 * the size of the leaf instance.
 */
#define _bru_thread_manager_malloc_thread(manager)                   \
    ((BruThread *) malloc(                                           \
         (manager)->table[bru_vt_leaf_idx(manager)]->_thread_size) + \
     (manager)->table[bru_vt_leaf_idx(manager)]->_thread_size)
#define _bru_thread_manager_free_thread(manager, thread) \
    free((thread) - (manager)->table[bru_vt_leaf_idx(manager)]->_thread_size)

#define bru_thread_manager_init_thread(manager, thread, pc, sp) \
    bru_vt_call_procedure(manager, init_thread, thread, pc, sp)
#define bru_thread_manager_copy_thread(manager, thread_src, thread_dst) \
    bru_vt_call_procedure(manager, copy_thread, thread_src, thread_dst)
#define bru_thread_manager_schedule_thread(manager, thread) \
    bru_vt_call_procedure(manager, schedule_thread, thread)
#define bru_thread_manager_schedule_thread_in_order(manager, thread) \
    bru_vt_call_procedure(manager, schedule_thread_in_order, thread)
#define bru_thread_manager_next_thread(manager) \
    bru_vt_call_function(manager, next_thread)
#define bru_thread_manager_notify_thread_match(manager, thread) \
    bru_vt_call_procedure(manager, notify_thread_match, thread)
#define bru_thread_manager_clone_thread(manager, thread) \
    bru_vt_call_function(manager, clone_thread, thread)
#define bru_thread_manager_kill_thread(manager, thread) \
    bru_vt_call_procedure(manager, kill_thread, thread)
#define bru_thread_manager_check_thread_eq(manager, thread1, thread2) \
    bru_vt_call_function(manager, check_thread_eq, thread1, thread2)

#define bru_thread_manager_pc(manager, thread) \
    bru_vt_call_function(manager, pc, thread)
#define bru_thread_manager_set_pc(manager, thread, pc) \
    bru_vt_call_procedure(manager, set_pc, thread, pc)
#define bru_thread_manager_sp(manager, thread) \
    bru_vt_call_function(manager, sp, thread)
#define bru_thread_manager_inc_sp(manager, thread) \
    bru_vt_call_procedure(manager, inc_sp, thread)

#define bru_thread_manager_init_memoisation(manager, nmemo, text_len) \
    bru_vt_call_procedure(manager, init_memoisation, nmemo, text_len)
#define bru_thread_manager_memoise_check(manager, thread, idx) \
    bru_vt_call_function(manager, memoise_check, thread, idx)
#define bru_thread_manager_memoise_set(manager, thread, idx) \
    bru_vt_call_procedure(manager, memoise_set, thread, idx)
#define bru_thread_manager_counter(manager, thread, idx) \
    bru_vt_call_function(manager, counter, thread, idx)
#define bru_thread_manager_set_counter(manager, thread, idx, val) \
    bru_vt_call_procedure(manager, set_counter, thread, idx, val)
#define bru_thread_manager_inc_counter(manager, thread, idx) \
    bru_vt_call_procedure(manager, inc_counter, thread, idx)
#define bru_thread_manager_memory(manager, thread, idx) \
    bru_vt_call_function(manager, memory, thread, idx)
#define bru_thread_manager_set_memory(manager, thread, idx, val, size) \
    bru_vt_call_procedure(manager, set_memory, thread, idx, val, size)
#define bru_thread_manager_bytes(manager, thread, nbytes) \
    bru_vt_call_function(manager, bytes, thread, nbytes)
#define bru_thread_manager_write_byte(manager, thread, byte) \
    bru_vt_call_procedure(manager, write_byte, thread, byte)
#define bru_thread_manager_captures(manager, thread, ncaptures) \
    bru_vt_call_function(manager, captures, thread, ncaptures)
#define bru_thread_manager_set_capture(manager, thread, idx) \
    bru_vt_call_procedure(manager, set_capture, thread, idx)
#define bru_thread_manager_capture_val(manager, thread, idx) \
    bru_vt_call_function(manager, capture_val, thread, idx)
#define bru_thread_manager_backref_index(manager, thread) \
    bru_vt_call_function(manager, backref_index, thread)
#define bru_thread_manager_set_backref_index(manager, thread, len) \
    bru_vt_call_procedure(manager, set_backref_index, thread, len)

#define BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(manager_interface, prefix)    \
    do {                                                                    \
        (manager_interface)->init      = prefix##_thread_manager_init;      \
        (manager_interface)->reset     = prefix##_thread_manager_reset;     \
        (manager_interface)->kill      = prefix##_thread_manager_kill;      \
        (manager_interface)->free      = prefix##_thread_manager_free;      \
        (manager_interface)->done_exec = prefix##_thread_manager_done_exec; \
        (manager_interface)->get_match = prefix##_thread_manager_get_match; \
                                                                            \
        (manager_interface)->alloc_thread =                                 \
            prefix##_thread_manager_alloc_thread;                           \
        (manager_interface)->spawn_thread =                                 \
            prefix##_thread_manager_spawn_thread;                           \
        (manager_interface)->init_thread =                                  \
            prefix##_thread_manager_init_thread;                            \
        (manager_interface)->copy_thread =                                  \
            prefix##_thread_manager_copy_thread;                            \
        (manager_interface)->clone_thread =                                 \
            prefix##_thread_manager_clone_thread;                           \
        (manager_interface)->kill_thread =                                  \
            prefix##_thread_manager_kill_thread;                            \
        (manager_interface)->free_thread =                                  \
            prefix##_thread_manager_free_thread;                            \
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
        (manager_interface)->memoise_check =                                  \
            bru_thread_manager_memoise_check_noop;                            \
        (manager_interface)->memoise_set =                                    \
            bru_thread_manager_memoise_set_noop;                              \
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
        (manager_interface)->backref_index =                                  \
            bru_thread_manager_backref_index_noop;                            \
        (manager_interface)->set_backref_index =                              \
            bru_thread_manager_set_backref_index_noop;                        \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef bru_byte_t BruThread; /**< BruThread is a collection of bytes         */

typedef bru_vtable_of(struct bru_thread_manager_interface) BruThreadManager;

typedef struct bru_thread_manager_interface {
    void (*init)(BruThreadManager *self,
                 const bru_byte_t *start_pc,
                 const char       *start_sp);
    void (*reset)(BruThreadManager *self);
    int (*done_exec)(BruThreadManager *self);
    BruThread *(*get_match)(BruThreadManager *self);

    /**
     * 'kill' is used to traverse the thread managers without removing them
     * from the hierarchy.
     *
     * 'free' is used to free the resources of the thread manager, and should
     * be called by using the bru_thread_manager_free function.
     *
     * The intention is that your base thread manager implements 'kill' by
     * deferring to bru_thread_manager_free.
     */
    void (*kill)(BruThreadManager *self); /**< kill the thread manager        */
    void (*free)(BruThreadManager *self); /**< free the thread manager        */

    // below functions manipulate thread execution
    BruThread *(*alloc_thread)(BruThreadManager *self);
    BruThread *(*spawn_thread)(BruThreadManager *self);
    void (*init_thread)(BruThreadManager *self,
                        BruThread        *thread,
                        const bru_byte_t *pc,
                        const char       *sp);
    void (*copy_thread)(BruThreadManager *self,
                        const BruThread  *src,
                        BruThread        *dst);
    BruThread *(*clone_thread)(BruThreadManager *self, const BruThread *thread);
    void (*kill_thread)(BruThreadManager *self, BruThread *thread);
    void (*free_thread)(BruThreadManager *self, BruThread *thread);

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
    int (*memoise_check)(BruThreadManager *self,
                         BruThread        *thread,
                         bru_len_t         idx);
    void (*memoise_set)(BruThreadManager *self,
                        BruThread        *thread,
                        bru_len_t         idx);

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
    bru_byte_t *(*bytes)(BruThreadManager *self,
                         BruThread        *thread,
                         size_t           *nbytes);
    void (*write_byte)(BruThreadManager *self,
                       BruThread        *thread,
                       bru_byte_t        byte);

    // captures
    const char *const *(*captures)(BruThreadManager *self,
                                   const BruThread  *thread,
                                   bru_len_t        *ncaptures);
    void (*set_capture)(BruThreadManager *self,
                        BruThread        *thread,
                        bru_len_t         idx);
    const char *(*capture_val)(BruThreadManager *self,
                               const BruThread  *thread,
                               bru_len_t         idx);

    // backrefs
    bru_len_t (*backref_index)(BruThreadManager *self, const BruThread *thread);
    void (*set_backref_index)(BruThreadManager *self,
                              BruThread        *thread,
                              bru_len_t         len);

    size_t _thread_size; /**< size of the thread used by this manager */
    BRU_VTABLE_FIELDS;
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
#    define thread_manager_kill      bru_thread_manager_kill
#    define thread_manager_done_exec bru_thread_manager_done_exec
#    define thread_manager_get_match bru_thread_manager_get_match

#    define thread_manager_schedule_thread bru_thread_manager_schedule_thread
#    define thread_manager_schedule_thread_in_order \
        bru_thread_manager_schedule_thread_in_order
#    define thread_manager_next_thread bru_thread_manager_next_thread
#    define thread_manager_notify_thread_match \
        bru_thread_manager_notify_thread_match
#    define thread_manager_alloc_thread    bru_thread_manager_alloc_thread
#    define thread_manager_spawn_thread    bru_thread_manager_spawn_thread
#    define thread_manager_init_thread     bru_thread_manager_init_thread
#    define thread_manager_check_thread_eq bru_thread_manager_check_thread_eq
#    define thread_manager_clone_thread    bru_thread_manager_clone_thread
#    define thread_manager_kill_thread     bru_thread_manager_kill_thread

#    define thread_manager_pc     bru_thread_manager_pc
#    define thread_manager_set_pc bru_thread_manager_set_pc
#    define thread_manager_sp     bru_thread_manager_sp
#    define thread_manager_inc_sp bru_thread_manager_inc_sp

#    define thread_manager_init_memoisation bru_thread_manager_init_memoisation
#    define thread_manager_memoise_check    bru_thread_manager_memoise_check
#    define thread_manager_memoise_set      bru_thread_manager_memoise_set
#    define thread_manager_counter          bru_thread_manager_counter
#    define thread_manager_set_counter      bru_thread_manager_set_counter
#    define thread_manager_inc_counter      bru_thread_manager_inc_counter
#    define thread_manager_memory           bru_thread_manager_memory
#    define thread_manager_set_memory       bru_thread_manager_set_memory
#    define thread_manager_captures         bru_thread_manager_captures
#    define thread_manager_set_capture      bru_thread_manager_set_capture
#    define thread_manager_bytes            bru_thread_manager_bytes
#    define thread_manager_write_byte       bru_thread_manager_write_byte

#    define THREAD_MANAGER_SET_REQUIRED_FUNCS \
        BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS
#    define THREAD_MANAGER_SET_ALL_FUNCS BRU_THREAD_MANAGER_SET_ALL_FUNCS

typedef BruThread                 Thread;
typedef BruThreadManager          ThreadManager;
typedef BruThreadManagerInterface ThreadManagerInterface;

#    define thread_manager_init_memoisation_noop \
        bru_thread_manager_init_memoisation_noop
#    define thread_manager_memoise_check_noop \
        bru_thread_manager_memoise_check_noop
#    define thread_manager_memoise_set_noop bru_thread_manager_memoise_set_noop
#    define thread_manager_counter_noop     bru_thread_manager_counter_noop
#    define thread_manager_set_counter_noop bru_thread_manager_set_counter_noop
#    define thread_manager_inc_counter_noop bru_thread_manager_inc_counter_noop
#    define thread_manager_memory_noop      bru_thread_manager_memory_noop
#    define thread_manager_set_memory_noop  bru_thread_manager_set_memory_noop
#    define thread_manager_captures_noop    bru_thread_manager_captures_noop
#    define thread_manager_set_capture_noop bru_thread_manager_set_capture_noop
#    define thread_manager_bytes_noop       bru_thread_manager_bytes_noop
#    define thread_manager_write_byte_noop  bru_thread_manager_write_byte_noop
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

int bru_thread_manager_memoise_check_noop(BruThreadManager *tm,
                                          BruThread        *thread,
                                          bru_len_t         idx);

void bru_thread_manager_memoise_set_noop(BruThreadManager *tm,
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
                                          size_t           *nbytes);

const char *const *bru_thread_manager_captures_noop(BruThreadManager *tm,
                                                    const BruThread  *thread,
                                                    bru_len_t *ncaptures);

void bru_thread_manager_set_capture_noop(BruThreadManager *tm,
                                         BruThread        *thread,
                                         bru_len_t         idx);

bru_len_t bru_thread_manager_backref_index_noop(BruThreadManager *tm,
                                                const BruThread  *thread);

void bru_thread_manager_set_backref_index_noop(BruThreadManager *tm,
                                               BruThread        *thread,
                                               bru_len_t         val);

#endif /* BRU_VM_THREAD_MANAGER_H */
