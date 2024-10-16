#include <stdlib.h>

#include "../../stc/fatp/vec.h"

#include "../../utils.h"
#include "lockstep.h"
#include "schedulers/lockstep.h"
#include "thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_thompson_thread {
    const bru_byte_t *pc;
    const char      **sp;
} BruThompsonThread;

typedef struct bru_thompson_thread_manager {
    BruScheduler     *scheduler; /**< Thompson scheduler for scheduling       */
    const bru_byte_t *start_pc;  /**< the starting PC for new threads         */
    const char       *start_sp;  /**< the starting SP for the current run     */
    const char       *sp;        /**< the string pointer for lockstep         */
    BruThread        *match;     /**< the matched thread                      */
} BruThompsonThreadManager;

/* --- ThompsonThreadManager function prototypes ---------------------------- */

static void       thompson_thread_manager_init(BruThreadManager *tm,
                                               const bru_byte_t *start_pc,
                                               const char       *start_sp);
static void       thompson_thread_manager_reset(BruThreadManager *tm);
static void       thompson_thread_manager_free(BruThreadManager *tm);
static void       thompson_thread_manager_kill(BruThreadManager *tm);
static int        thompson_thread_manager_done_exec(BruThreadManager *tm);
static BruThread *thompson_thread_manager_get_match(BruThreadManager *tm);

static BruThread *thompson_thread_manager_alloc_thread(BruThreadManager *tm);
static BruThread *thompson_thread_manager_spawn_thread(BruThreadManager *tm);
static void       thompson_thread_manager_init_thread(BruThreadManager *tm,
                                                      BruThread        *thread,
                                                      const bru_byte_t *pc,
                                                      const char       *sp);
static void       thompson_thread_manager_copy_thread(BruThreadManager *tm,
                                                      const BruThread  *src,
                                                      BruThread        *dst);
static int        thompson_thread_manager_check_thread_eq(BruThreadManager *tm,
                                                          const BruThread  *t1,
                                                          const BruThread  *t2);
static void       thompson_thread_manager_schedule_thread(BruThreadManager *tm,
                                                          BruThread        *t);
#define thompson_thread_manager_schedule_thread_in_order \
    thompson_thread_manager_schedule_thread
static BruThread *thompson_thread_manager_next_thread(BruThreadManager *tm);
static void thompson_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                        BruThread        *t);
static BruThread *thompson_thread_manager_clone_thread(BruThreadManager *tm,
                                                       const BruThread  *t);
static void       thompson_thread_manager_kill_thread(BruThreadManager *tm,
                                                      BruThread        *t);
static void       thompson_thread_manager_free_thread(BruThreadManager *tm,
                                                      BruThread        *t);

static const bru_byte_t *thompson_thread_manager_pc(BruThreadManager *tm,
                                                    const BruThread  *t);
static void              thompson_thread_manager_set_pc(BruThreadManager *tm,
                                                        BruThread        *t,
                                                        const bru_byte_t *pc);
static const char       *thompson_thread_manager_sp(BruThreadManager *tm,
                                                    const BruThread  *t);
static void thompson_thread_manager_inc_sp(BruThreadManager *tm, BruThread *t);

/* --- ThompsonThreadManager function definitions --------------------------- */

BruThreadManager *bru_thompson_thread_manager_new(void)
{
    BruThompsonThreadManager  *ttm = malloc(sizeof(*ttm));
    BruThreadManagerInterface *tmi =
        bru_thread_manager_interface_new(ttm, sizeof(BruThompsonThread));
    BruThreadManager *tm = malloc(sizeof(*tm));

    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_init(tm, tmi);

    ttm->scheduler = bru_lockstep_scheduler_new(tm);
    ttm->match     = NULL;

    BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(tmi, thompson);
    BRU_THREAD_MANAGER_SET_NOOP_FUNCS(tmi);

    return tm;
}

static void thompson_thread_manager_init(BruThreadManager *tm,
                                         const bru_byte_t *start_pc,
                                         const char       *start_sp)
{
    BruThompsonThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                *thread;

    self->start_pc = start_pc;
    self->sp = self->start_sp = start_sp;
    bru_scheduler_init(self->scheduler);
    if (self->match) {
        bru_thread_manager_kill_thread(tm, self->match);
        self->match = NULL;
    }

    bru_vt_call_function(tm, thread, alloc_thread);
    bru_thread_manager_init_thread(tm, thread, start_pc, start_sp);
    bru_thread_manager_schedule_thread(tm, thread);
}

static void thompson_thread_manager_reset(BruThreadManager *tm)
{
    BruThread                *t;
    BruThompsonThreadManager *self = bru_vt_curr_impl(tm);
    BruScheduler             *ts   = self->scheduler;

    while ((t = bru_scheduler_next(ts))) bru_thread_manager_kill_thread(tm, t);

    if (self->match) {
        bru_thread_manager_kill_thread(tm, self->match);
        self->match = NULL;
    }
}

static void thompson_thread_manager_free(BruThreadManager *tm)
{
    BruThompsonThreadManager *self = bru_vt_curr_impl(tm);

    bru_scheduler_free(self->scheduler);
    free(self);
}

static void thompson_thread_manager_kill(BruThreadManager *tm)
{
    bru_thread_manager_reset(tm);
    _bru_thread_manager_free(tm);
}

static int thompson_thread_manager_done_exec(BruThreadManager *tm)
{
    return *((BruThompsonThreadManager *) bru_vt_curr_impl(tm))->start_sp ==
           '\0';
}

static BruThread *thompson_thread_manager_get_match(BruThreadManager *tm)
{
    return ((BruThompsonThreadManager *) bru_vt_curr_impl(tm))->match;
}

static BruThread *thompson_thread_manager_alloc_thread(BruThreadManager *tm)
{
    return _bru_thread_manager_malloc_thread(tm);
}

static BruThread *thompson_thread_manager_spawn_thread(BruThreadManager *tm)
{
    BruThread *_t;
    return bru_vt_call_function(tm, _t, alloc_thread);
}

static void thompson_thread_manager_init_thread(BruThreadManager *tm,
                                                BruThread        *thread,
                                                const bru_byte_t *pc,
                                                const char       *sp)
{
    BRU_UNUSED(sp);
    BruThompsonThreadManager  *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);
    BruThompsonThread         *tt =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    tt->pc = pc;
    tt->sp = &self->sp;
}

static void thompson_thread_manager_copy_thread(BruThreadManager *tm,
                                                const BruThread  *src,
                                                BruThread        *dst)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThompsonThread         *tt_src =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, src);
    BruThompsonThread *tt_dst =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, dst);

    tt_dst->pc = tt_src->pc;
    tt_dst->sp = tt_src->sp;
}

static int thompson_thread_manager_check_thread_eq(BruThreadManager *tm,
                                                   const BruThread  *t1,
                                                   const BruThread  *t2)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThompsonThread         *tt1 =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, t1);
    BruThompsonThread *tt2 =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, t2);

    return tt1->pc == tt2->pc;
}

static void thompson_thread_manager_schedule_thread(BruThreadManager *tm,
                                                    BruThread        *t)
{
    BruThompsonThreadManager *self = bru_vt_curr_impl(tm);
    if (!bru_scheduler_schedule(self->scheduler, t))
        bru_thread_manager_kill_thread(tm, t);
}

static BruThread *thompson_thread_manager_next_thread(BruThreadManager *tm)
{
    BruThompsonThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                *thread;

    // advance the SP after a lockstep only if we still have threads to
    // execute, or we don't have a match yet in which case we spawn a new thread
    if (bru_lockstep_scheduler_done_step(self->scheduler) && *self->sp &&
        (!self->match || bru_scheduler_has_next(self->scheduler))) {
        self->sp = stc_utf8_str_next(self->sp);
        if (!self->match) {
            bru_vt_call_function(tm, thread, alloc_thread);
            bru_thread_manager_init_thread(tm, thread, self->start_pc,
                                           self->sp);
            bru_thread_manager_schedule_thread(tm, thread);
        }
    }

    return bru_scheduler_next(self->scheduler);
}

static void thompson_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                        BruThread        *t)
{
    BruThompsonThreadManager *self = bru_vt_curr_impl(tm);
    BruScheduler             *ts   = self->scheduler;
    BruThread               **low_priority_threads;
    size_t                    i, nthreads;

    if (self->match) bru_thread_manager_kill_thread(tm, self->match);
    self->match = t;

    low_priority_threads =
        bru_lockstep_scheduler_remove_low_priority_threads(ts);
    nthreads = stc_vec_len(low_priority_threads);
    for (i = 0; i < nthreads; i++)
        bru_thread_manager_kill_thread(tm, low_priority_threads[i]);
    stc_vec_free(low_priority_threads);
}

static BruThread *thompson_thread_manager_clone_thread(BruThreadManager *tm,
                                                       const BruThread  *t)
{
    BruThread *clone;

    bru_vt_call_function(tm, clone, alloc_thread);
    bru_thread_manager_copy_thread(tm, t, clone);

    return clone;
}

static void thompson_thread_manager_kill_thread(BruThreadManager *tm,
                                                BruThread        *t)
{
    bru_vt_call_procedure(tm, free_thread, t);
}

static void thompson_thread_manager_free_thread(BruThreadManager *tm,
                                                BruThread        *t)
{
    _bru_thread_manager_free_thread(tm, t);
}

/* --- Thread function definitions ------------------------------------------ */

static const bru_byte_t *thompson_thread_manager_pc(BruThreadManager *tm,
                                                    const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThompsonThread         *tt =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);

    return tt->pc;
}

static void thompson_thread_manager_set_pc(BruThreadManager *tm,
                                           BruThread        *t,
                                           const bru_byte_t *pc)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThompsonThread         *tt =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);

    tt->pc = pc;
}

static const char *thompson_thread_manager_sp(BruThreadManager *tm,
                                              const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThompsonThread         *tt =
        (BruThompsonThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);

    return *tt->sp;
}

static void thompson_thread_manager_inc_sp(BruThreadManager *tm, BruThread *t)
{
    BRU_UNUSED(tm);
    BRU_UNUSED(t);
}
