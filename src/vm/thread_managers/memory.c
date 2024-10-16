#include "memory.h"
#include <string.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    bru_len_t memlen; /**< the size of the extra memory                       */
} BruThreadManagerWithMemory;

/* --- Function prototypes -------------------------------------------------- */

static void thread_manager_with_memory_free(BruThreadManager *tm);

static void thread_init_with_memory(BruThreadManager *tm,
                                    BruThread        *thread,
                                    const bru_byte_t *pc,
                                    const char       *sp);
static void thread_copy_with_memory(BruThreadManager *tm,
                                    const BruThread  *src,
                                    BruThread        *dst);
static int  thread_check_eq_with_memory(BruThreadManager *tm,
                                        const BruThread  *t1,
                                        const BruThread  *t2);

static void *
thread_get_memory(BruThreadManager *tm, const BruThread *thread, bru_len_t idx);
static void thread_set_memory(BruThreadManager *tm,
                              BruThread        *thread,
                              bru_len_t         idx,
                              const void       *val,
                              size_t            size);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_thread_manager_with_memory_new(BruThreadManager *tm,
                                                     bru_len_t         memlen)
{
    BruThreadManagerInterface  *tmi, *super;
    BruThreadManagerWithMemory *impl = malloc(sizeof(*impl));

    // populate impl
    impl->memlen = memlen;

    // create thread manager instance
    super = bru_vt_curr(tm);
    tmi   = bru_thread_manager_interface_new(impl, memlen * sizeof(bru_byte_t) +
                                                       super->_thread_size);

    // store functions
    tmi->free            = thread_manager_with_memory_free;
    tmi->init_thread     = thread_init_with_memory;
    tmi->copy_thread     = thread_copy_with_memory;
    tmi->check_thread_eq = thread_check_eq_with_memory;
    tmi->memory          = thread_get_memory;
    tmi->set_memory      = thread_set_memory;

    // register extension
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruCountersManager function definitions ------------------------------ */

static void thread_manager_with_memory_free(BruThreadManager *tm)
{
    BruThreadManagerWithMemory *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);

    free(self);
    bru_vt_call_super_procedure(tm, tmi, free);
}

static void thread_init_with_memory(BruThreadManager *tm,
                                    BruThread        *thread,
                                    const bru_byte_t *pc,
                                    const char       *sp)
{
    BruThreadManagerWithMemory *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);
    bru_byte_t *memory = (bru_byte_t *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    memset(memory, 0, sizeof(*memory) * self->memlen);

    bru_vt_call_super_procedure(tm, tmi, init_thread, thread, pc, sp);
}

static void thread_copy_with_memory(BruThreadManager *tm,
                                    const BruThread  *src,
                                    BruThread        *dst)
{
    BruThreadManagerWithMemory *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);
    bru_byte_t *src_memory = (bru_byte_t *) BRU_THREAD_FROM_INSTANCE(tmi, src);
    bru_byte_t *dst_memory = (bru_byte_t *) BRU_THREAD_FROM_INSTANCE(tmi, dst);

    memcpy(dst_memory, src_memory, sizeof(*src_memory) * self->memlen);

    bru_vt_call_super_procedure(tm, tmi, copy_thread, src, dst);
}

static int thread_check_eq_with_memory(BruThreadManager *tm,
                                       const BruThread  *t1,
                                       const BruThread  *t2)
{
    BruThreadManagerWithMemory *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);
    bru_byte_t *mem1 = (bru_byte_t *) BRU_THREAD_FROM_INSTANCE(tmi, t1);
    bru_byte_t *mem2 = (bru_byte_t *) BRU_THREAD_FROM_INSTANCE(tmi, t2);
    int         _eq;

    return bru_vt_call_super_function(tm, tmi, _eq, check_thread_eq, t1, t2) &&
           memcmp(mem1, mem2, self->memlen * sizeof(*mem1)) == 0;
}

static void *
thread_get_memory(BruThreadManager *tm, const BruThread *thread, bru_len_t idx)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_byte_t *memory = (bru_byte_t *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    return memory + idx;
}

static void thread_set_memory(BruThreadManager *tm,
                              BruThread        *thread,
                              bru_len_t         idx,
                              const void       *val,
                              size_t            size)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_byte_t *memory = (bru_byte_t *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    memcpy(memory + idx, val, size);
}
