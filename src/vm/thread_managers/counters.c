#include "counters.h"
#include <string.h>

/* --- Data structures ------------------------------------------------------ */

typedef struct {
    bru_len_t ncounters; /**< number of counters to allocate space for        */
} BruThreadManagerWithCounters;

/* --- Function prototypes -------------------------------------------------- */

static void thread_manager_with_counters_free(BruThreadManager *tm);

static void thread_init_with_counters(BruThreadManager *tm,
                                      BruThread        *thread,
                                      const bru_byte_t *pc,
                                      const char       *sp);
static void thread_copy_with_counters(BruThreadManager *tm,
                                      const BruThread  *src,
                                      BruThread        *dst);
static int  thread_check_eq_with_counters(BruThreadManager *tm,
                                          const BruThread  *t1,
                                          const BruThread  *t2);

static bru_cntr_t thread_get_counter(BruThreadManager *tm,
                                     const BruThread  *thread,
                                     bru_len_t         idx);
static void       thread_set_counter(BruThreadManager *tm,
                                     BruThread        *thread,
                                     bru_len_t         idx,
                                     bru_cntr_t        val);
static void
thread_inc_counter(BruThreadManager *tm, BruThread *thread, bru_len_t idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_thread_manager_with_counters_new(BruThreadManager *tm,
                                                       bru_len_t ncounters)
{
    BruThreadManagerInterface    *tmi, *super;
    BruThreadManagerWithCounters *impl = malloc(sizeof(*impl));

    // populate impl
    impl->ncounters = ncounters;

    // create thread manager instance
    super = bru_vt_curr(tm);
    tmi   = bru_thread_manager_interface_new(
        impl, ncounters * sizeof(bru_cntr_t) + super->_thread_size);

    // store functions
    tmi->free            = thread_manager_with_counters_free;
    tmi->init_thread     = thread_init_with_counters;
    tmi->copy_thread     = thread_copy_with_counters;
    tmi->check_thread_eq = thread_check_eq_with_counters;
    tmi->counter         = thread_get_counter;
    tmi->set_counter     = thread_set_counter;
    tmi->inc_counter     = thread_inc_counter;

    // register extension
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruCountersManager function definitions ------------------------------ */

static void thread_manager_with_counters_free(BruThreadManager *tm)
{
    BruThreadManagerWithCounters *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);

    bru_vt_shrink(tm);
    bru_vt_call_procedure(tm, free);

    free(self);
    bru_thread_manager_interface_free(tmi);
}

static void thread_init_with_counters(BruThreadManager *tm,
                                      BruThread        *thread,
                                      const bru_byte_t *pc,
                                      const char       *sp)
{
    BruThreadManagerWithCounters *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);
    bru_cntr_t *counters = (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    memset(counters, 0, sizeof(*counters) * self->ncounters);

    bru_vt_call_super_procedure(tm, tmi, init_thread, thread, pc, sp);
}

static void thread_copy_with_counters(BruThreadManager *tm,
                                      const BruThread  *src,
                                      BruThread        *dst)
{
    BruThreadManagerWithCounters *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);
    bru_cntr_t                   *src_counters =
        (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, src);
    bru_cntr_t *dst_counters =
        (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, dst);

    memcpy(dst_counters, src_counters, sizeof(*src_counters) * self->ncounters);

    bru_vt_call_super_procedure(tm, tmi, copy_thread, src, dst);
}

static int thread_check_eq_with_counters(BruThreadManager *tm,
                                         const BruThread  *t1,
                                         const BruThread  *t2)
{
    BruThreadManagerWithCounters *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);
    bru_cntr_t *cntrs1 = (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, t1);
    bru_cntr_t *cntrs2 = (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, t2);
    size_t      i;
    int         eq;

    if (!bru_vt_call_super_function(tm, tmi, eq, check_thread_eq, t1, t2))
        return eq;

    for (i = 0; i < self->ncounters && (eq = cntrs1[i] == cntrs2[i]); i++);

    return eq;
}

static bru_cntr_t
thread_get_counter(BruThreadManager *tm, const BruThread *thread, bru_len_t idx)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_cntr_t *counters = (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    return counters[idx];
}

static void thread_set_counter(BruThreadManager *tm,
                               BruThread        *thread,
                               bru_len_t         idx,
                               bru_cntr_t        val)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_cntr_t *counters = (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    counters[idx] = val;
}

static void
thread_inc_counter(BruThreadManager *tm, BruThread *thread, bru_len_t idx)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_cntr_t *counters = (bru_cntr_t *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    counters[idx]++;
}
