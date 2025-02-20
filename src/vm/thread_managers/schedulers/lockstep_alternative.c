#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <bru/vm/thread_managers/schedulers/lockstep_alternative.h>

/* --- Preprocessor directives --------------------------------------------- */

// FIXME: cannot guarantee number of threads scheduled after step equals
// number of threads initially in self->locked
#define START_STEPPING(self) \
    (self)->state = BRU_LOCKSTEP_SCHEDULER_STATE_STEPPING

#define STOP_STEPPING(self) \
    (self)->state = BRU_LOCKSTEP_SCHEDULER_STATE_DONE_STEP

#define RESUME_NORMAL_EXECUTION(self) \
    (self)->state = BRU_LOCKSTEP_SCHEDULER_STATE_NORMAL

/* --- Type definitions ----------------------------------------------------- */

typedef enum {
    BRU_LOCKSTEP_SCHEDULER_STATE_NORMAL,   /**< normal execution              */
    BRU_LOCKSTEP_SCHEDULER_STATE_STEPPING, /**< providing threads from locked */
    BRU_LOCKSTEP_SCHEDULER_STATE_DONE_STEP /**< no more threads in locked     */
} BruLockstepSchedulerState;

typedef struct {
    BruThreadManager   *tm;     /**< the thread manager using the scheduler   */
    StcVec(BruThread *) locked; /**< queue of locked threads (state subset)   */

    BruThread          *active;         /**< currently active thread for DFS  */
    StcVec(BruThread *) stack;          /**< DFS stack for inbetween steps    */
    StcVec(BruThread *) in_order_queue; /**< queue to schedule in-order       */

    BruLockstepSchedulerState state; /**< current state of scheduling         */
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
static int lockstep_is_locking_thread(BruLockstepAltScheduler *self,
                                      BruThread               *t);

/* --- Lockstep function definitions ---------------------------------------- */

BruScheduler *bru_lockstep_alt_scheduler_new(BruThreadManager *tm)
{
    BruLockstepAltScheduler *las = malloc(sizeof(*las));
    BruScheduler            *s   = malloc(sizeof(*s));

    las->tm    = tm;
    las->state = BRU_LOCKSTEP_SCHEDULER_STATE_NORMAL;
    stc_vec_default_init(&las->locked); // NOLINT(bugprone-sizeof-expression)

    las->active = NULL;
    stc_vec_default_init(&las->stack); // NOLINT(bugprone-sizeof-expression)
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    stc_vec_default_init(&las->in_order_queue);

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
        stc_vec_init(&las->stack, stc_vec_cap(threads));
        if (las->active) {
            // NOLINTNEXTLINE(bugprone-sizeof-expression)
            stc_vec_push_back(&threads, las->active);
        }
    }

    return threads;
}

int bru_lockstep_alt_scheduler_done_step(BruScheduler *self)
{
    BruLockstepAltScheduler *las = self->impl;
    return las->state == BRU_LOCKSTEP_SCHEDULER_STATE_DONE_STEP;
}

/* --- LockstepScheduler function definitions ------------------------------- */

static void lockstep_alt_scheduler_init(void *impl)
{
    BruLockstepAltScheduler *self = impl;

    self->active = NULL;
    self->state  = BRU_LOCKSTEP_SCHEDULER_STATE_NORMAL;
}

static int lockstep_alt_scheduler_schedule(void *impl, BruThread *thread)
{
    BruLockstepAltScheduler *self = impl;

    switch (self->state) {
        case BRU_LOCKSTEP_SCHEDULER_STATE_NORMAL: goto normal;
        case BRU_LOCKSTEP_SCHEDULER_STATE_STEPPING: goto stepping;
        case BRU_LOCKSTEP_SCHEDULER_STATE_DONE_STEP: goto done_step;
    }

normal:
    if (self->active) {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(&self->stack, thread);
    } else if (lockstep_is_locking_thread(self, thread)) {
        if (lockstep_threads_contain(self->tm, self->locked, thread))
            return FALSE;
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(&self->locked, thread);
    } else {
        self->active = thread;
    }

    return TRUE;

stepping:
    // if stepping, we want to keep the original priority so schedule
    // in order
    return lockstep_alt_scheduler_schedule_in_order(impl, thread);

done_step:
    // this is the final 'locked' thread being rescheduled; do the same as if we
    // were still stepping
    goto stepping;
}

static int lockstep_alt_scheduler_schedule_in_order(void      *impl,
                                                    BruThread *thread)
{
    BruLockstepAltScheduler *self = impl;
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    stc_vec_push_back(&self->in_order_queue, thread);
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

    switch (self->state) {
        case BRU_LOCKSTEP_SCHEDULER_STATE_NORMAL: goto normal;
        case BRU_LOCKSTEP_SCHEDULER_STATE_STEPPING: goto stepping;
        case BRU_LOCKSTEP_SCHEDULER_STATE_DONE_STEP: goto done_step;
    }

normal:
    if (!stc_vec_is_empty(self->in_order_queue)) {
        while (stc_vec_len(self->in_order_queue) > 1)
            stc_vec_push_back(&self->stack,
                              stc_vec_pop_back(&self->in_order_queue));
        assert(self->active == NULL && "active is not null");
        self->active = stc_vec_pop_back(&self->in_order_queue);
    }

    // check if active thread can be used
    if (self->active) {
        if (lockstep_is_locking_thread(self, self->active)) {
            stc_vec_push_back(&self->locked, self->active);
            self->active = NULL;
        } else {
            thread       = self->active;
            self->active = NULL;
            return thread;
        }
    }

    // else check stack
    while (!stc_vec_is_empty(self->stack)) {
        thread = stc_vec_pop_back(&self->stack);
        if (lockstep_is_locking_thread(self, thread))
            stc_vec_push_back(&self->locked, thread);
        else
            return thread;
    }

    // otherwise, start stepping the threads in locked
    START_STEPPING(self);

stepping:
    // if no threads are in locked, we are out of threads
    if (stc_vec_is_empty(self->locked)) return NULL;

    thread = stc_vec_first(self->locked);
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    stc_vec_remove(self->locked, 0);
    if (stc_vec_is_empty(self->locked)) STOP_STEPPING(self);
    return thread;

done_step:
    RESUME_NORMAL_EXECUTION(self);
    goto normal;
}

static void lockstep_alt_scheduler_free(void *impl)
{
    BruLockstepAltScheduler *self = impl;
    stc_vec_free(self->stack);
    stc_vec_free(self->locked);
    stc_vec_free(self->in_order_queue);
    free(self);
}

static int lockstep_threads_contain(BruThreadManager   *tm,
                                    StcVec(BruThread *) threads,
                                    BruThread          *thread)
{
    size_t i, len;
    int    _cmp = FALSE;

    len = stc_vec_len(threads);
    for (i = 0; i < len && !bru_thread_manager_check_thread_eq(
                               tm, _cmp, threads[i], thread);
         i++);

    return _cmp;
}

static int lockstep_is_locking_thread(BruLockstepAltScheduler *self,
                                      BruThread               *thread)
{
    const bru_byte_t *_pc;
    bru_len_t         k;
    const char       *capture_start, *capture_end;

    if (!thread) return FALSE;

    switch ((BruBytecode) *bru_thread_manager_pc(self->tm, _pc, thread)) {
        case BRU_CHAR:
        case BRU_PRED: return TRUE;

        case BRU_BACKREF:
            bru_thread_manager_backref_index(self->tm, k, thread);
            bru_thread_manager_capture_val(self->tm, capture_start, thread,
                                           2 * k);
            bru_thread_manager_capture_val(self->tm, capture_end, thread,
                                           2 * k + 1);

            // capture not used
            if (!capture_start || !capture_end) return FALSE;
            assert(capture_start <= capture_end);

            // empty capture; nothing to backref
            if (capture_end - capture_start == 0) return FALSE;

            return TRUE;

        case BRU_NOOP:
        case BRU_MATCH:
        case BRU_BEGIN:
        case BRU_END:
        case BRU_JMP:
        case BRU_SPLIT:
        case BRU_GSPLIT:
        case BRU_LSPLIT:
        case BRU_TSWITCH:
        case BRU_SAVE:
        case BRU_INC:
        case BRU_SET:
        case BRU_CMP:
        case BRU_EPSRESET:
        case BRU_EPSSET:
        case BRU_EPSCHK:
        case BRU_MEMOSET:
        case BRU_MEMOCHK:
        case BRU_ZWA:
        case BRU_STATE:
        case BRU_WRITE:
        case BRU_WRITE0:
        case BRU_WRITE1:
        case BRU_NBYTECODES: return FALSE;
    }
}
