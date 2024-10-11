#ifndef BRU_VM_THREAD_H
#define BRU_VM_THREAD_H

#include "../../../types.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define bru_thread_init(thread, pc, sp) \
    (thread)->init((thread)->impl, (pc), (sp))
#define bru_thread_api_equal(t1, t2)                           \
    ((t1)->init == (t2)->init && (t1)->equal == (t2)->equal && \
     (t1)->copy == (t2)->copy && (t1)->free == (t2)->free &&   \
     (t1)->pc == (t2)->pc && (t1)->set_pc == (t2)->set_pc &&   \
     (t1)->sp == (t2)->sp && (t1)->inc_sp == (t2)->inc_sp)
#define bru_thread_equal(t1, t2) \
    (bru_thread_api_equal((t1), (t2)) && (t1)->equal((t1)->impl, (t2)->impl))
#define bru_thread_copy(src, dst)                  \
    do {                                           \
        if (bru_thread_api_equal((src), (dst)))    \
            (src)->copy((src)->impl, (dst)->impl); \
    } while (0)
#define bru_thread_free(thread)         \
    do {                                \
        (thread)->free((thread)->impl); \
        free((thread));                 \
    } while (0)

#define bru_thread_pc(thread)         (thread)->pc((thread)->impl)
#define bru_thread_set_pc(thread, pc) (thread)->set_pc((thread)->impl, (pc))
#define bru_thread_sp(thread)         (thread)->sp((thread)->impl)
#define bru_thread_inc_sp(thread)     (thread)->inc_sp((thread)->impl)

#define BRU_THREAD_SET_REQUIRED_FUNCS(thread, prefix) \
    do {                                              \
        (thread)->init  = prefix##_thread_init;       \
        (thread)->equal = prefix##_thread_equal;      \
        (thread)->copy  = prefix##_thread_copy;       \
        (thread)->free  = prefix##_thread_free;       \
                                                      \
        (thread)->pc     = prefix##_thread_pc;        \
        (thread)->set_pc = prefix##_thread_set_pc;    \
        (thread)->sp     = prefix##_thread_sp;        \
        (thread)->inc_sp = prefix##_thread_inc_sp;    \
    } while (0)

#define BRU_THREAD_SET_ALL_FUNCS BRU_THREAD_SET_REQUIRED_FUNCS

#define BRU_THREAD_FUNCTION_PROTOTYPES(prefix)                                   \
    static void prefix##_thread_init(void *, const bru_byte_t *,                 \
                                     const char *);                              \
    static int  prefix##_thread_equal(void *, void *);                           \
    static void prefix##_thread_copy(void *, void *);                            \
    static void prefix##_thread_free(void *);                                    \
                                                                                 \
    static const bru_byte_t *prefix##_thread_pc(void *);                         \
    static void              prefix##_thread_set_pc(void *, const bru_byte_t *); \
    static const char       *prefix##_thread_sp(void *);                         \
    static void              prefix##_thread_inc_sp(void *)

/**
 * Convenience macro for passing through the basic pc/sp functions of threads,
 * since most extensions will not touch these but will affect how threads are
 * free'd, initialised, copied, etc.
 */
#define BRU_THREAD_PC_SP_FUNCTION_PASSTHROUGH(prefix, supertype, member) \
    static const bru_byte_t *prefix##_thread_pc(void *impl)              \
    {                                                                    \
        supertype *self = impl;                                          \
        return bru_thread_pc(self->member);                              \
    }                                                                    \
                                                                         \
    static void prefix##_thread_set_pc(void *impl, const bru_byte_t *pc) \
    {                                                                    \
        supertype *self = impl;                                          \
        bru_thread_set_pc(self->member, pc);                             \
    }                                                                    \
                                                                         \
    static const char *prefix##_thread_sp(void *impl)                    \
    {                                                                    \
        supertype *self = impl;                                          \
        return bru_thread_sp(self->member);                              \
    }                                                                    \
                                                                         \
    static void prefix##_thread_inc_sp(void *impl)                       \
    {                                                                    \
        supertype *self = impl;                                          \
        bru_thread_inc_sp(self->member);                                 \
    }

#if !defined(BRU_VM_THREAD_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&       \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||    \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define thread_init  bru_thread_init
#    define thread_equal bru_thread_equal
#    define thread_copy  bru_thread_copy
#    define thread_free  bru_thread_free

#    define thread_pc     bru_thread_pc
#    define thread_set_pc bru_thread_set_pc
#    define thread_sp     bru_thread_sp
#    define thread_inc_sp bru_thread_inc_sp

#    define THREAD_SET_REQUIRED_FUNCS BRU_THREAD_SET_REQUIRED_FUNCS
#    define THREAD_SET_ALL_FUNCS      BRU_THREAD_SET_ALL_FUNCS

typedef BruThread Thread;
#endif /* BRU_VM_THREAD_ENABLE_SHORT_NAMES */

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_thread BruThread;

struct bru_thread {
    void (*init)(void *thread_impl, const bru_byte_t *pc, const char *sp);
    int (*equal)(void *t1_impl, void *t2_impl);
    void (*copy)(void *src_impl, void *dst_impl);
    void (*free)(void *thread_impl);

    const bru_byte_t *(*pc)(void *thread_impl);
    void (*set_pc)(void *thread_impl, const bru_byte_t *pc);
    const char *(*sp)(void *thread_impl);
    void (*inc_sp)(void *thread_impl);

    void *impl; /**< the implementation                                       */
};

#endif /* BRU_VM_THREAD_H */
