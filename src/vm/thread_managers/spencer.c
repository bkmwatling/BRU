#include <stdlib.h>

#include "schedulers/spencer.h"
#include "spencer.h"
#include "thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_spencer_thread {
    const bru_byte_t *pc;
    const char       *sp;
} BruSpencerThread;

typedef struct bru_spencer_thread_manager {
    BruScheduler *scheduler; /**< the Spencer scheduler for scheduling        */
    const char   *start_sp;  /**< the starting SP for the current run         */
} BruSpencerThreadManager;

/* --- SpencerThreadManager function prototypes ----------------------------- */

static void spencer_thread_manager_init(BruThreadManager *tm,
                                        const bru_byte_t *start_pc,
                                        const char       *start_sp);
static void spencer_thread_manager_reset(BruThreadManager *tm);
static void spencer_thread_manager_free(BruThreadManager *tm);
static int  spencer_thread_manager_done_exec(BruThreadManager *tm);

static BruThread *spencer_thread_manager_alloc_thread(BruThreadManager *tm);
static void       spencer_thread_manager_init_thread(BruThreadManager *tm,
                                                     BruThread        *thread,
                                                     const bru_byte_t *pc,
                                                     const char       *sp);
static void       spencer_thread_manager_copy_thread(BruThreadManager *tm,
                                                     const BruThread  *src,
                                                     BruThread        *dst);
static int        spencer_thread_manager_check_thread_eq(BruThreadManager *tm,
                                                         const BruThread  *t1,
                                                         const BruThread  *t2);
static void       spencer_thread_manager_schedule_thread(BruThreadManager *tm,
                                                         BruThread        *t);
static void
spencer_thread_manager_schedule_thread_in_order(BruThreadManager *tm,
                                                BruThread        *t);
static BruThread *spencer_thread_manager_next_thread(BruThreadManager *tm);
static void spencer_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                       BruThread        *t);
static BruThread *spencer_thread_manager_clone_thread(BruThreadManager *tm,
                                                      const BruThread  *t);
static void       spencer_thread_manager_kill_thread(BruThreadManager *tm,
                                                     BruThread        *t);
static const bru_byte_t *spencer_thread_manager_pc(BruThreadManager *tm,
                                                   const BruThread  *t);
static void              spencer_thread_manager_set_pc(BruThreadManager *tm,
                                                       BruThread        *t,
                                                       const bru_byte_t *pc);
static const char       *spencer_thread_manager_sp(BruThreadManager *tm,
                                                   const BruThread  *t);
static void spencer_thread_manager_inc_sp(BruThreadManager *tm, BruThread *t);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_spencer_thread_manager_new(void)
{
    BruSpencerThreadManager   *stm = malloc(sizeof(*stm));
    BruThreadManagerInterface *tmi =
        bru_thread_manager_interface_new(stm, sizeof(BruSpencerThread));
    BruThreadManager *tm = malloc(sizeof(*tm));

    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_init(tm, tmi);

    stm->scheduler = bru_spencer_scheduler_new();
    stm->start_sp  = NULL;

    BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(tmi, spencer);
    BRU_THREAD_MANAGER_SET_NOOP_FUNCS(tmi);

    return tm;
}

/* --- SpencerThreadManager function definitions ---------------------------- */

static BruThread *spencer_thread_manager_alloc_thread(BruThreadManager *tm)
{
    return bru_thread_manager_malloc_thread(tm);
}

static void spencer_thread_manager_init(BruThreadManager *tm,
                                        const bru_byte_t *start_pc,
                                        const char       *start_sp)
{
    BruSpencerThreadManager *self = bru_vt_curr_impl(tm);
    BruThread               *thread;

    self->start_sp = start_sp;
    bru_thread_manager_alloc_thread(tm, thread);
    bru_thread_manager_init_thread(tm, thread, start_pc, start_sp);
    bru_thread_manager_schedule_thread(tm, thread);
}

static void spencer_thread_manager_reset(BruThreadManager *tm)
{
    BruSpencerThreadManager *self = bru_vt_curr_impl(tm);
    BruThread               *t;

    while ((t = bru_scheduler_next(self->scheduler)))
        bru_thread_manager_kill_thread(tm, t);
}

static void spencer_thread_manager_free(BruThreadManager *tm)
{
    BruSpencerThreadManager   *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);

    bru_vt_shrink(tm);
    bru_scheduler_free(self->scheduler);
    free(self);
    bru_thread_manager_interface_free(tmi);
}

static int spencer_thread_manager_done_exec(BruThreadManager *tm)
{
    return *((BruSpencerThreadManager *) bru_vt_curr_impl(tm))->start_sp ==
           '\0';
}

static void spencer_thread_manager_init_thread(BruThreadManager *tm,
                                               BruThread        *thread,
                                               const bru_byte_t *pc,
                                               const char       *sp)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruSpencerThread          *st =
        (BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    st->pc = pc;
    st->sp = sp;
}

static void spencer_thread_manager_copy_thread(BruThreadManager *tm,
                                               const BruThread  *src,
                                               BruThread        *dst)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruSpencerThread          *st_src =
        (BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, src);
    BruSpencerThread *st_dst =
        (BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, dst);

    st_dst->sp = st_src->sp;
    st_dst->pc = st_src->pc;
}

static int spencer_thread_manager_check_thread_eq(BruThreadManager *tm,
                                                  const BruThread  *t1,
                                                  const BruThread  *t2)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruSpencerThread          *st1 =
        (BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, t1);
    BruSpencerThread *st2 =
        (BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, t2);

    return st1->pc == st2->pc && st1->sp == st2->sp;
}

static void spencer_thread_manager_schedule_thread(BruThreadManager *tm,
                                                   BruThread        *t)
{
    bru_scheduler_schedule(
        ((BruSpencerThreadManager *) bru_vt_curr_impl(tm))->scheduler, t);
}

static void
spencer_thread_manager_schedule_thread_in_order(BruThreadManager *tm,
                                                BruThread        *t)
{
    bru_scheduler_schedule_in_order(
        ((BruSpencerThreadManager *) bru_vt_curr_impl(tm))->scheduler, t);
}

static BruThread *spencer_thread_manager_next_thread(BruThreadManager *tm)
{
    return bru_scheduler_next(
        ((BruSpencerThreadManager *) bru_vt_curr_impl(tm))->scheduler);
}

static void spencer_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                       BruThread        *t)
{
    // empty the scheduler
    bru_thread_manager_kill_thread(tm, t);
    bru_thread_manager_reset(tm);
}

static BruThread *spencer_thread_manager_clone_thread(BruThreadManager *tm,
                                                      const BruThread  *t)
{
    BruThread *clone;

    bru_thread_manager_alloc_thread(tm, clone);
    bru_thread_manager_copy_thread(tm, t, clone);

    return clone;
}

static void spencer_thread_manager_kill_thread(BruThreadManager *tm,
                                               BruThread        *t)
{
    bru_thread_manager_free_thread(tm, t);
}

static const bru_byte_t *spencer_thread_manager_pc(BruThreadManager *tm,
                                                   const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    const BruSpencerThread    *st =
        (const BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    return st->pc;
}

static void spencer_thread_manager_set_pc(BruThreadManager *tm,
                                          BruThread        *t,
                                          const bru_byte_t *pc)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruSpencerThread          *st =
        (BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    st->pc = pc;
}

static const char *spencer_thread_manager_sp(BruThreadManager *tm,
                                             const BruThread  *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    const BruSpencerThread    *st =
        (const BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    return st->sp;
}

static void spencer_thread_manager_inc_sp(BruThreadManager *tm, BruThread *t)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruSpencerThread          *st =
        (BruSpencerThread *) BRU_THREAD_FROM_INSTANCE(tmi, t);
    st->sp = stc_utf8_str_next(st->sp);
}
