#include <stdlib.h>

#include "lockstep.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    BruThreadManager *tm; /**< the thread manager using this scheduler        */
    int in_lockstep; /**< whether to execute the synchronisation thread queue */

    BruThread **curr; /**< stc_vec for current queue of threads to execute    */
    BruThread **next; /**< stc_vec for next queue of threads to be executed   */
    BruThread **sync; /**< stc_vec for synchronisation queue for lockstep     */
} BruLockstepScheduler;

/* --- LockstepScheduler function prototypes -------------------------------- */

static void       lockstep_scheduler_init(void *impl);
static int        lockstep_scheduler_schedule(void *impl, BruThread *thread);
static int        lockstep_scheduler_has_next(const void *impl);
static BruThread *lockstep_scheduler_next(void *impl);
static void       lockstep_scheduler_free(void *impl);

/* --- Helper function prototypes ------------------------------------------- */

static int lockstep_threads_contain(BruThreadManager *tm,
                                    BruThread       **threads,
                                    BruThread        *thread);

/* --- Lockstep function prototypes ----------------------------------------- */

BruScheduler *bru_lockstep_scheduler_new(BruThreadManager *tm)
{
    BruLockstepScheduler *ts = malloc(sizeof(*ts));
    BruScheduler         *s  = malloc(sizeof(*s));

    ts->tm          = tm;
    ts->in_lockstep = FALSE;
    stc_vec_default_init(ts->curr); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(ts->next); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(ts->sync); // NOLINT(bugprone-sizeof-expression)

    s->impl              = ts;
    s->init              = lockstep_scheduler_init;
    s->schedule          = lockstep_scheduler_schedule;
    s->schedule_in_order = lockstep_scheduler_schedule;
    s->has_next          = lockstep_scheduler_has_next;
    s->next              = lockstep_scheduler_next;
    s->free              = lockstep_scheduler_free;

    return s;
}

BruThread **
bru_lockstep_scheduler_remove_low_priority_threads(BruScheduler *self)
{
    BruLockstepScheduler *ls      = self->impl;
    BruThread           **threads = NULL;
    size_t                ncurr   = stc_vec_len(ls->curr);
    size_t                i;

    if (ncurr) {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_init(threads, ncurr);

        for (i = 0; i < ncurr; i++) {
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            stc_vec_push_back(threads, stc_vec_pop(ls->curr));
        }
    }

    return threads;
}

int bru_lockstep_scheduler_done_step(BruScheduler *self)
{
    BruLockstepScheduler *ls = self->impl;
    return stc_vec_is_empty(ls->curr) && ls->in_lockstep;
}

/* --- LockstepScheduler function definitions ------------------------------- */

static void lockstep_scheduler_init(void *impl)
{
    BruLockstepScheduler *self = impl;

    self->in_lockstep = FALSE;
}

static int lockstep_scheduler_schedule(void *impl, BruThread *thread)
{
    BruLockstepScheduler *self = impl;
    const bru_byte_t     *_pc;

    if (lockstep_threads_contain(self->tm, self->sync, thread)) return FALSE;

    switch (*bru_thread_manager_pc(self->tm, _pc, thread)) {
        case BRU_CHAR:
        case BRU_PRED:
            if (stc_vec_is_empty(self->next))
                // NOLINTNEXTLINE(bugprone-sizeof-expression)
                stc_vec_push_back(self->sync, thread);
            else
                // NOLINTNEXTLINE(bugprone-sizeof-expression)
                stc_vec_push_back(self->next, thread);
            break;

        default:
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            stc_vec_push_back(self->next, thread);
            break;
    }
    return TRUE;
}

static int lockstep_scheduler_has_next(const void *impl)
{
    const BruLockstepScheduler *self = impl;
    return !(stc_vec_is_empty(self->curr) && stc_vec_is_empty(self->next) &&
             stc_vec_is_empty(self->sync));
}

static BruThread *lockstep_scheduler_next(void *impl)
{
    BruLockstepScheduler *self   = impl;
    BruThread            *thread = NULL;
    BruThread           **tmp;
    const bru_byte_t     *_pc;

lockstep_scheduler_next_start:
    if (stc_vec_is_empty(self->curr)) {
        if (stc_vec_is_empty(self->next)) {
            self->in_lockstep = TRUE;
            tmp               = self->curr;
            self->curr        = self->sync;
            self->sync        = tmp;
        } else {
            self->in_lockstep = FALSE;
            tmp               = self->curr;
            self->curr        = self->next;
            self->next        = tmp;
        }
    }

    if (!stc_vec_is_empty(self->curr)) {
        thread = self->curr[0];
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_remove(self->curr, 0);
        switch (*bru_thread_manager_pc(self->tm, _pc, thread)) {
            case BRU_CHAR:
            case BRU_PRED:
                if (!self->in_lockstep) {
                    // FIXME: memory leak on thread below if schedule returns
                    // FALSE
                    lockstep_scheduler_schedule(self, thread);
                    goto lockstep_scheduler_next_start;
                }
                break;

            default: break;
        }
    }

    return thread;
}

static void lockstep_scheduler_free(void *impl)
{
    BruLockstepScheduler *self = impl;
    stc_vec_free(self->curr);
    stc_vec_free(self->next);
    stc_vec_free(self->sync);
    free(self);
}

static int lockstep_threads_contain(BruThreadManager *tm,
                                    BruThread       **threads,
                                    BruThread        *thread)
{
    size_t i, len;
    int    _cmp;

    len = stc_vec_len(threads);
    for (i = 0; i < len; i++)
        if (bru_thread_manager_check_thread_eq(tm, _cmp, threads[i], thread))
            return TRUE;

    return FALSE;
}
