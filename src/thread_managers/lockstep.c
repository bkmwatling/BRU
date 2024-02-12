#include <stdlib.h>
#include <string.h>

#include "../stc/fatp/slice.h"
#include "../stc/fatp/vec.h"
#include "../utils.h"

#include "lockstep.h"
#include "thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct thompson_thread {
    const byte  *pc;
    const char  *sp;
    cntr_t      *counters; /*<< stc_slice                                     */
    byte        *memory;   /*<< stc_slice                                     */
    const char **captures; /*<< stc_slice                                     */
} ThompsonThread;

typedef struct thompson_scheduler {
    const char *text;
    int         in_sync;

    Thread **curr; /*<< stc_vec                                       */
    Thread **next; /*<< stc_vec                                       */
    Thread **sync; /*<< stc_vec                                       */
} ThompsonScheduler;

typedef struct thompson_thread_manager {
    ThompsonScheduler *scheduler;

    // for spawning threads
    len_t ncounters;
    len_t memory_len;
    len_t ncaptures;
} ThompsonThreadManager;

/* --- ThreadManager function prototypes ------------------------------------ */

static void thompson_thread_manager_reset(void *impl);
static void thompson_thread_manager_free(void *impl);

static Thread *thompson_thread_manager_spawn_thread(void       *impl,
                                                    const byte *pc,
                                                    const char *sp);
static void    thompson_thread_manager_schedule_thread(void *impl, Thread *t);
static void    thompson_thread_manager_schedule_thread_in_order(void   *impl,
                                                                Thread *t);
static Thread *thompson_thread_manager_next_thread(void *impl);
static void thompson_thread_manager_notify_thread_match(void *impl, Thread *t);
static Thread *thompson_thread_manager_clone_thread(void         *impl,
                                                    const Thread *t);
static void    thompson_thread_manager_kill_thread(void *impl, Thread *t);

static const byte *thompson_thread_pc(void *impl, const Thread *t);
static void thompson_thread_set_pc(void *impl, Thread *t, const byte *pc);
static const char *thompson_thread_sp(void *impl, const Thread *t);
static void        thompson_thread_inc_sp(void *impl, Thread *t);
static cntr_t thompson_thread_counter(void *impl, const Thread *t, len_t idx);
static void
thompson_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val);
static void  thompson_thread_inc_counter(void *impl, Thread *t, len_t idx);
static void *thompson_thread_memory(void *impl, const Thread *t, len_t idx);
static void  thompson_thread_set_memory(void       *impl,
                                        Thread     *t,
                                        len_t       idx,
                                        const void *val,
                                        size_t      size);
static const char *const *
thompson_thread_captures(void *impl, const Thread *t, len_t *ncaptures);
static void thompson_thread_set_capture(void *impl, Thread *t, len_t idx);

/* --- Scheduler function prototypes ---------------------------------------- */

static ThompsonScheduler *thompson_scheduler_new(void);
static int thompson_scheduler_schedule(ThompsonScheduler *self, Thread *thread);
static Thread *thompson_scheduler_next(ThompsonScheduler *self);
static void    thompson_scheduler_free(ThompsonScheduler *self);

/* --- ThompsonThreadManager function definitions --------------------------- */

ThreadManager *
thompson_thread_manager_new(len_t ncounters, len_t memory_len, len_t ncaptures)
{
    ThreadManager         *tm  = malloc(sizeof(*tm));
    ThompsonThreadManager *stm = malloc(sizeof(*stm));

    stm->scheduler  = thompson_scheduler_new();
    stm->ncounters  = ncounters;
    stm->memory_len = memory_len;
    stm->ncaptures  = ncaptures;

    THREAD_MANAGER_SET_REQUIRED_FUNCS(tm, thompson);

    // TODO: create thread managers for below optional implementations
    tm->counter     = thompson_thread_counter;
    tm->set_counter = thompson_thread_set_counter;
    tm->inc_counter = thompson_thread_inc_counter;
    tm->memory      = thompson_thread_memory;
    tm->set_memory  = thompson_thread_set_memory;
    tm->captures    = thompson_thread_captures;
    tm->set_capture = thompson_thread_set_capture;

    // opt out of memoisation
    tm->init_memoisation = thread_manager_init_memoisation_noop;
    tm->memoise          = thread_manager_memoise_noop;

    tm->impl = stm;

    return tm;
}

static void thompson_thread_manager_reset(void *impl)
{
    Thread            *t;
    ThompsonScheduler *ts = ((ThompsonThreadManager *) impl)->scheduler;

    for (t = thompson_scheduler_next(ts); t; t = thompson_scheduler_next(ts))
        thompson_thread_manager_kill_thread(impl, t);
}

static void thompson_thread_manager_free(void *impl)
{
    thompson_thread_manager_reset(impl);
    thompson_scheduler_free(((ThompsonThreadManager *) impl)->scheduler);
    free(impl);
}

static Thread *
thompson_thread_manager_spawn_thread(void *impl, const byte *pc, const char *sp)
{
    ThompsonThread        *tt   = malloc(sizeof(*tt));
    ThompsonThreadManager *self = (ThompsonThreadManager *) impl;

    tt->pc = pc;
    tt->sp = sp;

    stc_slice_init(tt->counters, self->ncounters);
    memset(tt->counters, 0, self->ncounters * sizeof(*tt->counters));
    stc_slice_init(tt->memory, self->memory_len);
    memset(tt->memory, 0, self->memory_len * sizeof(*tt->memory));
    stc_slice_init(tt->captures, 2 * self->ncaptures);
    memset(tt->captures, 0, 2 * self->ncaptures * sizeof(*tt->captures));

    return (Thread *) tt;
}

static void thompson_thread_manager_schedule_thread(void *impl, Thread *t)
{
    if (!thompson_scheduler_schedule(
            ((ThompsonThreadManager *) impl)->scheduler, t)) {
        thompson_thread_manager_kill_thread(impl, t);
    }
}

static void thompson_thread_manager_schedule_thread_in_order(void   *impl,
                                                             Thread *t)
{
    thompson_thread_manager_schedule_thread(impl, t);
}

static Thread *thompson_thread_manager_next_thread(void *impl)
{
    return thompson_scheduler_next(((ThompsonThreadManager *) impl)->scheduler);
}

static void thompson_thread_manager_notify_thread_match(void *impl, Thread *t)
{
    ThompsonScheduler *ts = ((ThompsonThreadManager *) impl)->scheduler;
    size_t             i, len = stc_vec_len(ts->curr);

    thompson_thread_manager_kill_thread(impl, t);
    for (i = 0; i < len; i++)
        if (ts->curr[i]) thompson_thread_manager_kill_thread(impl, ts->curr[i]);
    stc_vec_clear(ts->curr);
}

static Thread *thompson_thread_manager_clone_thread(void *impl, const Thread *t)
{
    ThompsonThread *clone = malloc(sizeof(*clone));
    memcpy(clone, t, sizeof(*clone));

    UNUSED(impl);
    clone->captures = stc_slice_clone(((ThompsonThread *) t)->captures);
    clone->counters = stc_slice_clone(((ThompsonThread *) t)->counters);
    clone->memory   = stc_slice_clone(((ThompsonThread *) t)->memory);

    return (Thread *) clone;
}

static void thompson_thread_manager_kill_thread(void *impl, Thread *t)
{
    UNUSED(impl);
    stc_slice_free(((ThompsonThread *) t)->captures);
    stc_slice_free(((ThompsonThread *) t)->counters);
    stc_slice_free(((ThompsonThread *) t)->memory);
    free(t);
}

/* --- Thread function definitions ---------------------------------- */
static const byte *thompson_thread_pc(void *impl, const Thread *t)
{
    UNUSED(impl);
    return t->pc;
}

static void thompson_thread_set_pc(void *impl, Thread *t, const byte *pc)
{
    UNUSED(impl);
    t->pc = pc;
}

static const char *thompson_thread_sp(void *impl, const Thread *t)
{
    UNUSED(impl);
    return t->sp;
}

static void thompson_thread_inc_sp(void *impl, Thread *t)
{
    UNUSED(impl);
    t->sp = stc_utf8_str_next(t->sp);
}

static cntr_t thompson_thread_counter(void *impl, const Thread *t, len_t idx)
{
    UNUSED(impl);
    return ((ThompsonThread *) t)->counters[idx];
}

static void
thompson_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val)
{
    UNUSED(impl);
    ((ThompsonThread *) t)->counters[idx] = val;
}

static void thompson_thread_inc_counter(void *impl, Thread *t, len_t idx)
{
    UNUSED(impl);
    ((ThompsonThread *) t)->counters[idx]++;
}

static void *thompson_thread_memory(void *impl, const Thread *t, len_t idx)
{
    UNUSED(impl);
    return ((ThompsonThread *) t)->memory + idx;
}

static void thompson_thread_set_memory(void       *impl,
                                       Thread     *t,
                                       len_t       idx,
                                       const void *val,
                                       size_t      size)
{
    UNUSED(impl);
    memcpy(((ThompsonThread *) t)->memory + idx, val, size);
}

static const char *const *
thompson_thread_captures(void *impl, const Thread *t, len_t *ncaptures)
{
    UNUSED(impl);
    if (ncaptures)
        *ncaptures = stc_slice_len(((ThompsonThread *) t)->captures) / 2;
    return ((ThompsonThread *) t)->captures;
}

static void thompson_thread_set_capture(void *impl, Thread *t, len_t idx)
{
    UNUSED(impl);
    ((ThompsonThread *) t)->captures[idx] = t->sp;
}

static int thompson_thread_eq(Thread *t1, Thread *t2)
{
    ThompsonThread *tt1 = (ThompsonThread *) t1, *tt2 = (ThompsonThread *) t2;
    len_t           i, len = stc_slice_len(tt1->counters);
    int eq = tt1->pc == tt2->pc && len == stc_slice_len(tt2->counters);

    for (i = 0; i < len && eq; i++) eq = tt1->counters[i] == tt2->counters[i];

    return eq;
}

static int thompson_threads_contain(Thread **threads, Thread *thread)
{
    size_t i, len;

    len = stc_vec_len(threads);
    for (i = 0; i < len; i++)
        if (thompson_thread_eq(threads[i], thread)) return TRUE;

    return FALSE;
}

/* --- ThompsonScheduler function definitions ------------------------------- */

static ThompsonScheduler *thompson_scheduler_new(void)
{
    ThompsonScheduler *ts = malloc(sizeof(*ts));

    ts->in_sync = FALSE;
    stc_vec_default_init(ts->curr); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(ts->next); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(ts->sync); // NOLINT(bugprone-sizeof-expression)

    return ts;
}

static int thompson_scheduler_schedule(ThompsonScheduler *self, Thread *thread)
{
    if (thompson_threads_contain(self->next, thread) ||
        thompson_threads_contain(self->sync, thread))
        return FALSE;

    switch (*thread->pc) {
        case CHAR:
        case PRED:
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

static Thread *thompson_scheduler_next(ThompsonScheduler *self)
{
    Thread  *thread = NULL;
    Thread **tmp;

    if (stc_vec_is_empty(self->curr)) {
        if (self->in_sync) self->in_sync = FALSE;

        if (stc_vec_is_empty(self->next)) {
            self->in_sync = TRUE;
            tmp           = self->curr;
            self->curr    = self->sync;
            self->sync    = tmp;
        } else {
            tmp        = self->curr;
            self->curr = self->next;
            self->next = tmp;
        }
    }

    if (!stc_vec_is_empty(self->curr)) {
        thread = self->curr[0];
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_remove(self->curr, 0);
        switch (*thread->pc) {
            case CHAR:
            case PRED:
                if (!self->in_sync) {
                    thompson_scheduler_schedule(self, thread);
                    thread = thompson_scheduler_next(self);
                }
                break;

            default: break;
        }
    }

    return thread;
}

static void thompson_scheduler_free(ThompsonScheduler *self)
{
    stc_vec_free(self->curr);
    stc_vec_free(self->next);
    stc_vec_free(self->sync);
    free(self);
}
