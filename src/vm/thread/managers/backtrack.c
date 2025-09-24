#include <stdbool.h>
#include <stdlib.h>

#include <bru/vm/thread/managers/backtrack.h>
#include <bru/vm/thread/managers/manager.h>
#include <bru/vm/thread/schedulers/backtrack.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_backtrack_thread {
    const bru_byte_t *pc;
    const char       *sp;
} BruBacktrackThread;

typedef struct bru_backtrack_tm {
    BruThreadScheduler *ts;       /**< the backtrack scheduler for scheduling */
    const char         *start_sp; /**< the starting SP for the current run    */
    BruThread          *match;    /**< the matched thread                     */
} BruBacktrackThreadManager;

/* --- BacktrackThreadManager function prototypes --------------------------- */

static void       backtrack_tm_init(BruThreadManager *tm,
                                    const bru_byte_t *start_pc,
                                    const char       *start_sp);
static void       backtrack_tm_reset(BruThreadManager *tm);
static void       backtrack_tm_free(BruThreadManager *tm);
static void       backtrack_tm_kill(BruThreadManager *tm);
static bool       backtrack_tm_done_exec(BruThreadManager *tm);
static BruThread *backtrack_tm_get_match(BruThreadManager *tm);

static BruThread *backtrack_tm_alloc_thread(BruThreadManager *tm);
static BruThread *backtrack_tm_spawn_thread(BruThreadManager *tm);
static void       backtrack_tm_init_thread(BruThreadManager *tm,
                                           BruThread        *thread,
                                           const bru_byte_t *pc,
                                           const char       *sp);
static void       backtrack_tm_copy_thread(BruThreadManager *tm,
                                           const BruThread  *src,
                                           BruThread        *dst);
static bool       backtrack_tm_check_thread_eq(BruThreadManager *tm,
                                               const BruThread  *t1,
                                               const BruThread  *t2);
static void backtrack_tm_schedule_thread(BruThreadManager *tm, BruThread *t);
static void backtrack_tm_schedule_thread_in_order(BruThreadManager *tm,
                                                  BruThread        *t);
static BruThread *backtrack_tm_next_thread(BruThreadManager *tm);
static void       backtrack_tm_notify_thread_match(BruThreadManager *tm,
                                                   BruThread        *t);
static BruThread *backtrack_tm_clone_thread(BruThreadManager *tm,
                                            const BruThread  *t);
static void       backtrack_tm_kill_thread(BruThreadManager *tm, BruThread *t);
static void       backtrack_tm_free_thread(BruThreadManager *tm, BruThread *t);

static const bru_byte_t *backtrack_tm_pc(BruThreadManager *tm,
                                         const BruThread  *t);
static void
backtrack_tm_set_pc(BruThreadManager *tm, BruThread *t, const bru_byte_t *pc);
static const char *backtrack_tm_sp(BruThreadManager *tm, const BruThread *t);
static void        backtrack_tm_inc_sp(BruThreadManager *tm, BruThread *t);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_backtrack_tm_new(void)
{
    BruBacktrackThreadManager *btm = malloc(sizeof(*btm));
    BruThreadManagerInterface *tmi =
        bru_tmi_new(btm, sizeof(BruBacktrackThread));
    BruThreadManager *tm = malloc(sizeof(*tm));

    bru_vt_init(tm, tmi);

    btm->ts       = bru_backtrack_ts_new();
    btm->match    = NULL;
    btm->start_sp = NULL;

    BRU_TM_SET_REQUIRED_FUNCS(tmi, backtrack);
    BRU_TM_SET_NOOP_FUNCS(tmi);

    return tm;
}

/* --- BacktrackThreadManager function definitions -------------------------- */

static BruThread *backtrack_tm_alloc_thread(BruThreadManager *tm)
{
    return _bru_tm_malloc_thread(tm);
}

static BruThread *backtrack_tm_spawn_thread(BruThreadManager *tm)
{
    return bru_vt_call_function(tm, alloc_thread);
}

static void backtrack_tm_init(BruThreadManager *tm,
                              const bru_byte_t *start_pc,
                              const char       *start_sp)
{
    BruBacktrackThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                 *thread;

    self->start_sp = start_sp;

    if (self->match) {
        bru_tm_kill_thread(tm, self->match);
        self->match = NULL;
    }
    thread = bru_vt_call_function(tm, spawn_thread);
    bru_tm_init_thread(tm, thread, start_pc, start_sp);
    bru_tm_schedule_thread(tm, thread);
}

static void backtrack_tm_reset(BruThreadManager *tm)
{
    BruBacktrackThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                 *t;

    while ((t = bru_ts_next(self->ts))) bru_tm_kill_thread(tm, t);

    if (self->match) {
        bru_tm_kill_thread(tm, self->match);
        self->match = NULL;
    }
}

static void backtrack_tm_free(BruThreadManager *tm)
{
    BruBacktrackThreadManager *self = bru_vt_curr_impl(tm);

    bru_ts_free(self->ts);
    free(self);
}

static void backtrack_tm_kill(BruThreadManager *tm)
{
    bru_tm_reset(tm);
    _bru_tm_free(tm);
}

static bool backtrack_tm_done_exec(BruThreadManager *tm)
{
    return *((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->start_sp ==
           '\0';
}

static BruThread *backtrack_tm_get_match(BruThreadManager *tm)
{
    return ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->match;
}

static void backtrack_tm_init_thread(BruThreadManager *tm,
                                     BruThread        *thread,
                                     const bru_byte_t *pc,
                                     const char       *sp)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *bt  = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    bt->pc = pc;
    bt->sp = sp;
}

static void backtrack_tm_copy_thread(BruThreadManager *tm,
                                     const BruThread  *src,
                                     BruThread        *dst)
{
    BruThreadManagerInterface *tmi    = bru_vt_curr(tm);
    BruBacktrackThread        *bt_src = BRU_THREAD_FROM_INSTANCE(tmi, src);
    BruBacktrackThread        *bt_dst = BRU_THREAD_FROM_INSTANCE(tmi, dst);

    bt_dst->sp = bt_src->sp;
    bt_dst->pc = bt_src->pc;
}

static bool backtrack_tm_check_thread_eq(BruThreadManager *tm,
                                         const BruThread  *t1,
                                         const BruThread  *t2)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *bt1 = BRU_THREAD_FROM_INSTANCE(tmi, t1);
    BruBacktrackThread        *bt2 = BRU_THREAD_FROM_INSTANCE(tmi, t2);

    return bt1->pc == bt2->pc && bt1->sp == bt2->sp;
}

static void backtrack_tm_schedule_thread(BruThreadManager *tm, BruThread *t)
{
    bru_ts_schedule(((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->ts,
                    t);
}

static void backtrack_tm_schedule_thread_in_order(BruThreadManager *tm,
                                                  BruThread        *t)
{
    bru_ts_schedule_in_order(
        ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->ts, t);
}

static BruThread *backtrack_tm_next_thread(BruThreadManager *tm)
{
    return bru_ts_next(
        ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->ts);
}

static void backtrack_tm_notify_thread_match(BruThreadManager *tm, BruThread *t)
{
    // empty the scheduler
    bru_tm_reset(tm);
    ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->match = t;
}

static BruThread *backtrack_tm_clone_thread(BruThreadManager *tm,
                                            const BruThread  *t)
{
    BruThread *clone;

    clone = bru_vt_call_function(tm, spawn_thread);
    bru_tm_copy_thread(tm, t, clone);

    return clone;
}

static void backtrack_tm_kill_thread(BruThreadManager *tm, BruThread *t)
{
    bru_vt_call_procedure(tm, free_thread, t);
}

static void backtrack_tm_free_thread(BruThreadManager *tm, BruThread *t)
{
    _bru_tm_free_thread(tm, t);
}

static const bru_byte_t *backtrack_tm_pc(BruThreadManager *tm,
                                         const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    const BruBacktrackThread  *bt  = BRU_THREAD_FROM_INSTANCE(tmi, t);
    return bt->pc;
}

static void
backtrack_tm_set_pc(BruThreadManager *tm, BruThread *t, const bru_byte_t *pc)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *bt  = BRU_THREAD_FROM_INSTANCE(tmi, t);
    bt->pc                         = pc;
}

static const char *backtrack_tm_sp(BruThreadManager *tm, const BruThread *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    const BruBacktrackThread  *bt  = BRU_THREAD_FROM_INSTANCE(tmi, t);
    return bt->sp;
}

static void backtrack_tm_inc_sp(BruThreadManager *tm, BruThread *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *bt  = BRU_THREAD_FROM_INSTANCE(tmi, t);
    bt->sp                         = stc_utf8_str_next(bt->sp);
}
