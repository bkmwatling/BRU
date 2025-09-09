#include <stdlib.h>

#include <stc/fatp/vec.h>

#include <bru/utils.h>
#include <bru/vm/thread/managers/lockstep.h>
#include <bru/vm/thread/managers/manager.h>
#include <bru/vm/thread/schedulers/lockstep.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    const bru_byte_t *pc;
} BruLockstepThread;

typedef struct {
    BruScheduler     *scheduler; /**< lockstep scheduler for scheduling       */
    const bru_byte_t *start_pc;  /**< the starting PC for new threads         */
    const char       *start_sp;  /**< the starting SP for the current run     */
    const char       *sp;        /**< the string pointer for lockstep         */
    BruThread        *match;     /**< the matched thread                      */
} BruLockstepThreadManager;

/* --- LockstepThreadManager function prototypes ---------------------------- */

static void       lockstep_tm_init(BruThreadManager *tm,
                                   const bru_byte_t *start_pc,
                                   const char       *start_sp);
static void       lockstep_tm_reset(BruThreadManager *tm);
static void       lockstep_tm_free(BruThreadManager *tm);
static void       lockstep_tm_kill(BruThreadManager *tm);
static int        lockstep_tm_done_exec(BruThreadManager *tm);
static BruThread *lockstep_tm_get_match(BruThreadManager *tm);

static BruThread *lockstep_tm_alloc_thread(BruThreadManager *tm);
static BruThread *lockstep_tm_spawn_thread(BruThreadManager *tm);
static void       lockstep_tm_init_thread(BruThreadManager *tm,
                                          BruThread        *thread,
                                          const bru_byte_t *pc,
                                          const char       *sp);
static void       lockstep_tm_copy_thread(BruThreadManager *tm,
                                          const BruThread  *src,
                                          BruThread        *dst);
static int        lockstep_tm_check_thread_eq(BruThreadManager *tm,
                                              const BruThread  *t1,
                                              const BruThread  *t2);
static void lockstep_tm_schedule_thread(BruThreadManager *tm, BruThread *t);
#define lockstep_tm_schedule_thread_in_order lockstep_tm_schedule_thread
static BruThread *lockstep_tm_next_thread(BruThreadManager *tm);
static void lockstep_tm_notify_thread_match(BruThreadManager *tm, BruThread *t);
static BruThread *lockstep_tm_clone_thread(BruThreadManager *tm,
                                           const BruThread  *t);
static void       lockstep_tm_kill_thread(BruThreadManager *tm, BruThread *t);
static void       lockstep_tm_free_thread(BruThreadManager *tm, BruThread *t);

static const bru_byte_t *lockstep_tm_pc(BruThreadManager *tm,
                                        const BruThread  *t);
static void
lockstep_tm_set_pc(BruThreadManager *tm, BruThread *t, const bru_byte_t *pc);
static const char *lockstep_tm_sp(BruThreadManager *tm, const BruThread *t);
static void        lockstep_tm_inc_sp(BruThreadManager *tm, BruThread *t);

/* --- LockstepThreadManager function definitions --------------------------- */

BruThreadManager *bru_lockstep_tm_new(void)
{
    BruLockstepThreadManager  *ltm = malloc(sizeof(*ltm));
    BruThreadManagerInterface *tmi =
        bru_tm_interface_new(ltm, sizeof(BruLockstepThread));
    BruThreadManager *tm = malloc(sizeof(*tm));

    bru_vt_init(tm, tmi);

    ltm->scheduler = bru_lockstep_scheduler_new(tm);
    ltm->match     = NULL;

    BRU_TM_SET_REQUIRED_FUNCS(tmi, lockstep);
    BRU_TM_SET_NOOP_FUNCS(tmi);

    return tm;
}

static void lockstep_tm_init(BruThreadManager *tm,
                             const bru_byte_t *start_pc,
                             const char       *start_sp)
{
    BruLockstepThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                *thread;

    self->start_pc = start_pc;
    self->sp = self->start_sp = start_sp;
    bru_scheduler_init(self->scheduler);
    if (self->match) {
        bru_tm_kill_thread(tm, self->match);
        self->match = NULL;
    }

    thread = bru_vt_call_function(tm, spawn_thread);
    bru_tm_init_thread(tm, thread, start_pc, start_sp);
    bru_tm_schedule_thread(tm, thread);
}

static void lockstep_tm_reset(BruThreadManager *tm)
{
    BruThread                *t;
    BruLockstepThreadManager *self = bru_vt_curr_impl(tm);
    BruScheduler             *ts   = self->scheduler;

    while ((t = bru_scheduler_next(ts))) bru_tm_kill_thread(tm, t);

    if (self->match) {
        bru_tm_kill_thread(tm, self->match);
        self->match = NULL;
    }
}

static void lockstep_tm_free(BruThreadManager *tm)
{
    BruLockstepThreadManager *self = bru_vt_curr_impl(tm);

    bru_scheduler_free(self->scheduler);
    free(self);
}

static void lockstep_tm_kill(BruThreadManager *tm)
{
    bru_tm_reset(tm);
    _bru_tm_free(tm);
}

static int lockstep_tm_done_exec(BruThreadManager *tm)
{
    return *((BruLockstepThreadManager *) bru_vt_curr_impl(tm))->start_sp ==
           '\0';
}

static BruThread *lockstep_tm_get_match(BruThreadManager *tm)
{
    return ((BruLockstepThreadManager *) bru_vt_curr_impl(tm))->match;
}

static BruThread *lockstep_tm_alloc_thread(BruThreadManager *tm)
{
    return _bru_tm_malloc_thread(tm);
}

static BruThread *lockstep_tm_spawn_thread(BruThreadManager *tm)
{
    return bru_vt_call_function(tm, alloc_thread);
}

static void lockstep_tm_init_thread(BruThreadManager *tm,
                                    BruThread        *thread,
                                    const bru_byte_t *pc,
                                    const char       *sp)
{
    BRU_UNUSED(sp);
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruLockstepThread         *lt  = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    lt->pc = pc;
}

static void lockstep_tm_copy_thread(BruThreadManager *tm,
                                    const BruThread  *src,
                                    BruThread        *dst)
{
    BruThreadManagerInterface *tmi    = bru_vt_curr(tm);
    BruLockstepThread         *lt_src = BRU_THREAD_FROM_INSTANCE(tmi, src);
    BruLockstepThread         *lt_dst = BRU_THREAD_FROM_INSTANCE(tmi, dst);

    lt_dst->pc = lt_src->pc;
}

static int lockstep_tm_check_thread_eq(BruThreadManager *tm,
                                       const BruThread  *t1,
                                       const BruThread  *t2)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruLockstepThread         *lt1 = BRU_THREAD_FROM_INSTANCE(tmi, t1);
    BruLockstepThread         *lt2 = BRU_THREAD_FROM_INSTANCE(tmi, t2);

    return lt1->pc == lt2->pc;
}

static void lockstep_tm_schedule_thread(BruThreadManager *tm, BruThread *t)
{
    BruLockstepThreadManager *self = bru_vt_curr_impl(tm);
    if (!bru_scheduler_schedule(self->scheduler, t)) bru_tm_kill_thread(tm, t);
}

static BruThread *lockstep_tm_next_thread(BruThreadManager *tm)
{
    BruLockstepThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                *thread;

    // advance the SP after a lockstep only if we still have threads to
    // execute, or we don't have a match yet in which case we spawn a new thread
    if (bru_lockstep_scheduler_done_step(self->scheduler) && *self->sp &&
        (!self->match || bru_scheduler_has_next(self->scheduler))) {
        self->sp = stc_utf8_str_next(self->sp);
        if (!self->match) {
            thread = bru_vt_call_function(tm, spawn_thread);
            bru_tm_init_thread(tm, thread, self->start_pc, self->sp);
            bru_tm_schedule_thread(tm, thread);
        }
    }

    return bru_scheduler_next(self->scheduler);
}

static void lockstep_tm_notify_thread_match(BruThreadManager *tm, BruThread *t)
{
    BruLockstepThreadManager *self = bru_vt_curr_impl(tm);
    BruScheduler             *ts   = self->scheduler;
    StcVec(BruThread *)       low_priority_threads;
    size_t                    i, nthreads;

    if (self->match) bru_tm_kill_thread(tm, self->match);
    self->match = t;

    low_priority_threads =
        bru_lockstep_scheduler_remove_low_priority_threads(ts);
    if ((nthreads =
             low_priority_threads ? stc_vec_len(low_priority_threads) : 0)) {
        for (i = 0; i < nthreads; i++)
            bru_tm_kill_thread(tm, low_priority_threads[i]);
        stc_vec_free(low_priority_threads);
    }
}

static BruThread *lockstep_tm_clone_thread(BruThreadManager *tm,
                                           const BruThread  *t)
{
    BruThread *clone;

    clone = bru_vt_call_function(tm, spawn_thread);
    bru_tm_copy_thread(tm, t, clone);

    return clone;
}

static void lockstep_tm_kill_thread(BruThreadManager *tm, BruThread *t)
{
    bru_vt_call_procedure(tm, free_thread, t);
}

static void lockstep_tm_free_thread(BruThreadManager *tm, BruThread *t)
{
    _bru_tm_free_thread(tm, t);
}

/* --- LockstepThread function definitions ---------------------------------- */

static const bru_byte_t *lockstep_tm_pc(BruThreadManager *tm,
                                        const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruLockstepThread         *lt  = BRU_THREAD_FROM_INSTANCE(tmi, t);

    return lt->pc;
}

static void
lockstep_tm_set_pc(BruThreadManager *tm, BruThread *t, const bru_byte_t *pc)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruLockstepThread         *lt  = BRU_THREAD_FROM_INSTANCE(tmi, t);

    lt->pc = pc;
}

static const char *lockstep_tm_sp(BruThreadManager *tm, const BruThread *t)
{
    BRU_UNUSED(t);
    BruLockstepThreadManager *self = bru_vt_curr_impl(tm);

    return self->sp;
}

static void lockstep_tm_inc_sp(BruThreadManager *tm, BruThread *t)
{
    BRU_UNUSED(tm);
    BRU_UNUSED(t);
}
