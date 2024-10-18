#include <stdio.h>
#include <stdlib.h>

#include "lockstep_alternative.h"

/* --- Preprocessor directives --------------------------------------------- */

// FIXME: cannot guarantee number of threads scheduled after step equals
// number of threads initially in self->locked
#define START_STEPPING(self)     \
    do {                         \
        (self)->stepping = TRUE; \
    } while (0)

#define STOP_STEPPING(self)        \
    do {                           \
        (self)->stepping  = FALSE; \
        (self)->done_step = TRUE;  \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    BruThreadManager   *tm;     /**< the thread manager using the scheduler   */
    StcVec(BruThread *) locked; /**< queue of locked threads (state subset)   */

    BruThread          *active;         /**< currently active thread for DFS  */
    StcVec(BruThread *) stack;          /**< DFS stack for inbetween steps    */
    StcVec(BruThread *) in_order_queue; /**< queue to schedule in-order       */

    bru_byte_t stepping;  /**< if we are moving threads out of locked         */
    bru_byte_t done_step; /**< if we just finished stepping                   */
} BruLockstepAltScheduler;

/* --- LockstepScheduler function prototypes -------------------------------- */

static void lockstep_alt_scheduler_init(void *impl);
static int  lockstep_alt_scheduler_schedule(void *impl, BruThread *thread);
static int  lockstep_alt_scheduler_schedule_in_order(void      *impl,
                                                     BruThread *thread);
static int  lockstep_alt_scheduler_has_next(const void *impl);
static BruThread *lockstep_alt_scheduler_next(void *impl);
static void       lockstep_alt_scheduler_free(void *impl);

/* --- Helper function prototypes ------------------------------------------- */

static int lockstep_threads_contain(BruThreadManager   *tm,
                                    StcVec(BruThread *) threads,
                                    BruThread          *thread);

/* --- Lockstep function definitions ---------------------------------------- */

BruScheduler *bru_lockstep_alt_scheduler_new(BruThreadManager *tm)
{
    BruLockstepAltScheduler *las = malloc(sizeof(*las));
    BruScheduler            *s   = malloc(sizeof(*s));

    las->tm        = tm;
    las->done_step = FALSE;
    las->stepping  = FALSE;
    stc_vec_default_init(las->locked); // NOLINT(bugprone-sizeof-expression)

    las->active = NULL;
    stc_vec_default_init(las->stack); // NOLINT(bugprone-sizeof-expression)
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    stc_vec_default_init(las->in_order_queue);

    s->impl              = las;
    s->init              = lockstep_alt_scheduler_init;
    s->schedule          = lockstep_alt_scheduler_schedule;
    s->schedule_in_order = lockstep_alt_scheduler_schedule_in_order;
    s->has_next          = lockstep_alt_scheduler_has_next;
    s->next              = lockstep_alt_scheduler_next;
    s->free              = lockstep_alt_scheduler_free;

    return s;
}

StcVec(BruThread *)
bru_lockstep_alt_scheduler_remove_low_priority_threads(BruScheduler *self)
{
    BruLockstepAltScheduler *las     = self->impl;
    StcVec(BruThread *)      threads = NULL;

    if (las->active || !stc_vec_is_empty(las->stack)) {
        threads = las->stack;
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_init(las->stack, stc_vec_len(threads));
        if (las->active) {
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            stc_vec_push_back(threads, las->active);
        }
    }

    return threads;
}

int bru_lockstep_alt_scheduler_done_step(BruScheduler *self)
{
    BruLockstepAltScheduler *las = self->impl;
    return las->done_step;
}

/* --- LockstepScheduler function definitions ------------------------------- */

static void lockstep_alt_scheduler_init(void *impl)
{
    BruLockstepAltScheduler *self = impl;

    self->active    = NULL;
    self->stepping  = FALSE;
    self->done_step = FALSE;
}

static int lockstep_alt_scheduler_schedule(void *impl, BruThread *thread)
{
    BruLockstepAltScheduler *self = impl;
    const bru_byte_t        *_pc;

    // FIXME: cannot guarantee number of threads scheduled after step equals
    // number of threads initially in self->locked
    if (self->stepping) {
        // if stepping, we want to keep the original priority so schedule
        // in order
        lockstep_alt_scheduler_schedule_in_order(impl, thread);
    } else {
        switch (*bru_thread_manager_pc(self->tm, _pc, thread)) {
            case BRU_CHAR: /* fallthrough */
            case BRU_PRED:
                if (lockstep_threads_contain(self->tm, self->locked, thread))
                    return FALSE;
                // NOLINTNEXTLINE(bugprone-sizeof-expression)
                stc_vec_push_back(self->locked, thread);
                break;

            default:
                if (self->active)
                    // NOLINTNEXTLINE(bugprone-sizeof-expression)
                    stc_vec_push_back(self->stack, thread);
                else
                    self->active = thread;
                break;
        }
    }

    return TRUE;
}

static int lockstep_alt_scheduler_schedule_in_order(void      *impl,
                                                    BruThread *thread)
{
    BruLockstepAltScheduler *self = impl;
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    stc_vec_push_back(self->in_order_queue, thread);
    return TRUE;
}

static int lockstep_alt_scheduler_has_next(const void *impl)
{
    const BruLockstepAltScheduler *self = impl;
    return !(self->active == NULL && stc_vec_is_empty(self->stack) &&
             stc_vec_is_empty(self->in_order_queue) &&
             stc_vec_is_empty(self->locked));
}

static BruThread *lockstep_alt_scheduler_next(void *impl)
{
    BruLockstepAltScheduler *self = impl;
    BruThread               *thread;
    const bru_byte_t        *_pc;

    if (!stc_vec_is_empty(self->in_order_queue)) {
        // TODO: move threads from in_order_queue to stack if:
        //          1) done_step is true OR
        //          2) stepping is false
    }
    if (self->done_step) self->done_step = FALSE;

    if (self->active == NULL && stc_vec_is_empty(self->stack)) {
        if (stc_vec_is_empty(self->locked)) {
            // no threads left
            return NULL;
        }
        // FIXME: cannot guarantee number of threads scheduled after step equals
        // number of threads initially in self->locked

        // else all threads present in self->locked; initiate stepping procedure
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        START_STEPPING(self);
    } else if (!self->stepping) {
        thread = self->active;
        if (thread)
            self->active = NULL;
        else
            thread = stc_vec_pop(self->stack);

        while (thread &&
               (*bru_thread_manager_pc(self->tm, _pc, thread) == BRU_CHAR ||
                *_pc == BRU_PRED)) {
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            stc_vec_push_back(self->locked, thread);
            thread =
                stc_vec_is_empty(self->stack) ? NULL : stc_vec_pop(self->stack);
        }
        if (thread == NULL) {
            // FIXME: cannot guarantee number of threads scheduled after step
            // equals number of threads initially in self->locked all threads in
            // scheduler are pred/char and moved to self->locked
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            START_STEPPING(self);
        }
    }

    if (self->stepping) {
        thread = stc_vec_first(self->locked);
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_remove(self->locked, 0);
        if (stc_vec_is_empty(self->locked)) STOP_STEPPING(self);
    }

    return thread;
}

static void lockstep_alt_scheduler_free(void *impl)
{
    BruLockstepAltScheduler *self = impl;
    stc_vec_free(self->stack);
    stc_vec_free(self->locked);
    free(self);
}

static int lockstep_threads_contain(BruThreadManager   *tm,
                                    StcVec(BruThread *) threads,
                                    BruThread          *thread)
{
    size_t i, len;
    int    _cmp = FALSE;

    len = stc_vec_len(threads);
    for (i = 0; i < len && bru_thread_manager_check_thread_eq(
                               tm, _cmp, threads[i], thread);
         i++);

    return _cmp;
}
