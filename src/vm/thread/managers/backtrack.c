#include <stdlib.h>

#include <bru/vm/thread/managers/backtrack.h>
#include <bru/vm/thread/managers/manager.h>
#include <bru/vm/thread/schedulers/backtrack.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_backtrack_thread {
    const bru_byte_t *pc;
    const char       *sp;
} BruBacktrackThread;

typedef struct bru_backtrack_thread_manager {
    BruScheduler *scheduler; /**< the backtrack scheduler for scheduling      */
    const char   *start_sp;  /**< the starting SP for the current run         */
    BruThread    *match;     /**< the matched thread                          */
} BruBacktrackThreadManager;

/* --- BacktrackThreadManager function prototypes --------------------------- */

static void       backtrack_thread_manager_init(BruThreadManager *tm,
                                                const bru_byte_t *start_pc,
                                                const char       *start_sp);
static void       backtrack_thread_manager_reset(BruThreadManager *tm);
static void       backtrack_thread_manager_free(BruThreadManager *tm);
static void       backtrack_thread_manager_kill(BruThreadManager *tm);
static int        backtrack_thread_manager_done_exec(BruThreadManager *tm);
static BruThread *backtrack_thread_manager_get_match(BruThreadManager *tm);

static BruThread *backtrack_thread_manager_alloc_thread(BruThreadManager *tm);
static BruThread *backtrack_thread_manager_spawn_thread(BruThreadManager *tm);
static void       backtrack_thread_manager_init_thread(BruThreadManager *tm,
                                                       BruThread        *thread,
                                                       const bru_byte_t *pc,
                                                       const char       *sp);
static void       backtrack_thread_manager_copy_thread(BruThreadManager *tm,
                                                       const BruThread  *src,
                                                       BruThread        *dst);
static int        backtrack_thread_manager_check_thread_eq(BruThreadManager *tm,
                                                           const BruThread  *t1,
                                                           const BruThread  *t2);
static void       backtrack_thread_manager_schedule_thread(BruThreadManager *tm,
                                                           BruThread        *t);
static void
backtrack_thread_manager_schedule_thread_in_order(BruThreadManager *tm,
                                                  BruThread        *t);
static BruThread *backtrack_thread_manager_next_thread(BruThreadManager *tm);
static void backtrack_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                         BruThread        *t);
static BruThread *backtrack_thread_manager_clone_thread(BruThreadManager *tm,
                                                        const BruThread  *t);
static void       backtrack_thread_manager_kill_thread(BruThreadManager *tm,
                                                       BruThread        *t);
static void       backtrack_thread_manager_free_thread(BruThreadManager *tm,
                                                       BruThread        *t);

static const bru_byte_t *backtrack_thread_manager_pc(BruThreadManager *tm,
                                                     const BruThread  *t);
static void              backtrack_thread_manager_set_pc(BruThreadManager *tm,
                                                         BruThread        *t,
                                                         const bru_byte_t *pc);
static const char       *backtrack_thread_manager_sp(BruThreadManager *tm,
                                                     const BruThread  *t);
static void backtrack_thread_manager_inc_sp(BruThreadManager *tm, BruThread *t);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_backtrack_thread_manager_new(void)
{
    BruBacktrackThreadManager *stm = malloc(sizeof(*stm));
    BruThreadManagerInterface *tmi =
        bru_thread_manager_interface_new(stm, sizeof(BruBacktrackThread));
    BruThreadManager *tm = malloc(sizeof(*tm));

    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_init(tm, tmi);

    stm->scheduler = bru_backtrack_scheduler_new();
    stm->match     = NULL;
    stm->start_sp  = NULL;

    BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(tmi, backtrack);
    BRU_THREAD_MANAGER_SET_NOOP_FUNCS(tmi);

    return tm;
}

/* --- BacktrackThreadManager function definitions -------------------------- */

static BruThread *backtrack_thread_manager_alloc_thread(BruThreadManager *tm)
{
    return _bru_thread_manager_malloc_thread(tm);
}

static BruThread *backtrack_thread_manager_spawn_thread(BruThreadManager *tm)
{
    return bru_vt_call_function(tm, alloc_thread);
}

static void backtrack_thread_manager_init(BruThreadManager *tm,
                                          const bru_byte_t *start_pc,
                                          const char       *start_sp)
{
    BruBacktrackThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                 *thread;

    self->start_sp = start_sp;

    if (self->match) {
        bru_thread_manager_kill_thread(tm, self->match);
        self->match = NULL;
    }
    thread = bru_vt_call_function(tm, spawn_thread);
    bru_thread_manager_init_thread(tm, thread, start_pc, start_sp);
    bru_thread_manager_schedule_thread(tm, thread);
}

static void backtrack_thread_manager_reset(BruThreadManager *tm)
{
    BruBacktrackThreadManager *self = bru_vt_curr_impl(tm);
    BruThread                 *t;

    while ((t = bru_scheduler_next(self->scheduler)))
        bru_thread_manager_kill_thread(tm, t);

    if (self->match) {
        bru_thread_manager_kill_thread(tm, self->match);
        self->match = NULL;
    }
}

static void backtrack_thread_manager_free(BruThreadManager *tm)
{
    BruBacktrackThreadManager *self = bru_vt_curr_impl(tm);

    bru_scheduler_free(self->scheduler);
    free(self);
}

static void backtrack_thread_manager_kill(BruThreadManager *tm)
{
    bru_thread_manager_reset(tm);
    _bru_thread_manager_free(tm);
}

static int backtrack_thread_manager_done_exec(BruThreadManager *tm)
{
    return *((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->start_sp ==
           '\0';
}

static BruThread *backtrack_thread_manager_get_match(BruThreadManager *tm)
{
    return ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->match;
}

static void backtrack_thread_manager_init_thread(BruThreadManager *tm,
                                                 BruThread        *thread,
                                                 const bru_byte_t *pc,
                                                 const char       *sp)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *st =
        (BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    st->pc = pc;
    st->sp = sp;
}

static void backtrack_thread_manager_copy_thread(BruThreadManager *tm,
                                                 const BruThread  *src,
                                                 BruThread        *dst)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *st_src =
        (BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, src);
    BruBacktrackThread *st_dst =
        (BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, dst);

    st_dst->sp = st_src->sp;
    st_dst->pc = st_src->pc;
}

static int backtrack_thread_manager_check_thread_eq(BruThreadManager *tm,
                                                    const BruThread  *t1,
                                                    const BruThread  *t2)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *st1 =
        (BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, t1);
    BruBacktrackThread *st2 =
        (BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, t2);

    return st1->pc == st2->pc && st1->sp == st2->sp;
}

static void backtrack_thread_manager_schedule_thread(BruThreadManager *tm,
                                                     BruThread        *t)
{
    bru_scheduler_schedule(
        ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->scheduler, t);
}

static void
backtrack_thread_manager_schedule_thread_in_order(BruThreadManager *tm,
                                                  BruThread        *t)
{
    bru_scheduler_schedule_in_order(
        ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->scheduler, t);
}

static BruThread *backtrack_thread_manager_next_thread(BruThreadManager *tm)
{
    return bru_scheduler_next(
        ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->scheduler);
}

static void backtrack_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                         BruThread        *t)
{
    // empty the scheduler
    bru_thread_manager_reset(tm);
    ((BruBacktrackThreadManager *) bru_vt_curr_impl(tm))->match = t;
}

static BruThread *backtrack_thread_manager_clone_thread(BruThreadManager *tm,
                                                        const BruThread  *t)
{
    BruThread *clone;

    clone = bru_vt_call_function(tm, spawn_thread);
    bru_thread_manager_copy_thread(tm, t, clone);

    return clone;
}

static void backtrack_thread_manager_kill_thread(BruThreadManager *tm,
                                                 BruThread        *t)
{
    bru_vt_call_procedure(tm, free_thread, t);
}

static void backtrack_thread_manager_free_thread(BruThreadManager *tm,
                                                 BruThread        *t)
{
    _bru_thread_manager_free_thread(tm, t);
}

static const bru_byte_t *backtrack_thread_manager_pc(BruThreadManager *tm,
                                                     const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    const BruBacktrackThread  *st =
        (const BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    return st->pc;
}

static void backtrack_thread_manager_set_pc(BruThreadManager *tm,
                                            BruThread        *t,
                                            const bru_byte_t *pc)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *st =
        (BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    st->pc = pc;
}

static const char *backtrack_thread_manager_sp(BruThreadManager *tm,
                                               const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    const BruBacktrackThread  *st =
        (const BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    return st->sp;
}

static void backtrack_thread_manager_inc_sp(BruThreadManager *tm, BruThread *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruBacktrackThread        *st =
        (BruBacktrackThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    st->sp = stc_utf8_str_next(st->sp);
}
