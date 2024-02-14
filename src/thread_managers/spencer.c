#include <stdlib.h>
#include <string.h>

#include "../stc/fatp/slice.h"
#include "../stc/fatp/vec.h"
#include "../utils.h"

#include "spencer.h"
#include "thread_manager.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct spencer_thread {
    const byte  *pc;
    const char  *sp;
    cntr_t      *counters; /*<< stc_slice                                     */
    byte        *memory;   /*<< stc_slice                                     */
    const char **captures; /*<< stc_slice                                     */
} SpencerThread;

typedef struct spencer_scheduler {
    size_t   in_order_idx;
    Thread  *active;
    Thread **stack; /*<< stc_vec                                       */
} SpencerScheduler;

typedef struct spencer_thread_manager {
    SpencerScheduler *scheduler;

    // for spawning threads
    len_t ncounters;
    len_t memory_len;
    len_t ncaptures;
} SpencerThreadManager;

/* --- SpencerThreadManager function prototypes ----------------------------- */

static void spencer_thread_manager_reset(void *impl);
static void spencer_thread_manager_free(void *impl);

static Thread *
spencer_thread_manager_spawn_thread(void *impl, const byte *pc, const char *sp);
static void    spencer_thread_manager_schedule_thread(void *impl, Thread *t);
static void    spencer_thread_manager_schedule_thread_in_order(void   *impl,
                                                               Thread *t);
static Thread *spencer_thread_manager_next_thread(void *impl);
static void spencer_thread_manager_notify_thread_match(void *impl, Thread *t);
static Thread *spencer_thread_manager_clone_thread(void *impl, const Thread *t);
static void    spencer_thread_manager_kill_thread(void *impl, Thread *t);
static const byte *spencer_thread_pc(void *impl, const Thread *t);
static void        spencer_thread_set_pc(void *impl, Thread *t, const byte *pc);
static const char *spencer_thread_sp(void *impl, const Thread *t);
static void        spencer_thread_inc_sp(void *impl, Thread *t);
static cntr_t spencer_thread_counter(void *impl, const Thread *t, len_t idx);
static void
spencer_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val);
static void  spencer_thread_inc_counter(void *impl, Thread *t, len_t idx);
static void *spencer_thread_memory(void *impl, const Thread *t, len_t idx);
static void  spencer_thread_set_memory(void       *impl,
                                       Thread     *t,
                                       len_t       idx,
                                       const void *val,
                                       size_t      size);
static const char *const *
spencer_thread_captures(void *impl, const Thread *t, len_t *ncaptures);
static void spencer_thread_set_capture(void *impl, Thread *t, len_t idx);

/* --- Scheduler function prototypes ---------------------------------------- */

static SpencerScheduler *spencer_scheduler_new(void);
static void spencer_scheduler_schedule(SpencerScheduler *self, Thread *thread);
static void spencer_scheduler_schedule_in_order(SpencerScheduler *self,
                                                Thread           *thread);
static Thread *spencer_scheduler_next(SpencerScheduler *self);
static void    spencer_scheduler_free(SpencerScheduler *self);

/* --- SpencerThreadManager function definitions ---------------------------- */

ThreadManager *
spencer_thread_manager_new(len_t ncounters, len_t memory_len, len_t ncaptures)
{
    ThreadManager        *tm  = malloc(sizeof(*tm));
    SpencerThreadManager *stm = malloc(sizeof(*stm));

    stm->scheduler  = spencer_scheduler_new();
    stm->ncounters  = ncounters;
    stm->memory_len = memory_len;
    stm->ncaptures  = ncaptures;

    THREAD_MANAGER_SET_REQUIRED_FUNCS(tm, spencer);

    // TODO: create thread managers for below optional implementations
    tm->counter     = spencer_thread_counter;
    tm->set_counter = spencer_thread_set_counter;
    tm->inc_counter = spencer_thread_inc_counter;
    tm->memory      = spencer_thread_memory;
    tm->set_memory  = spencer_thread_set_memory;
    tm->captures    = spencer_thread_captures;
    tm->set_capture = spencer_thread_set_capture;

    // opt out of memoisation
    tm->init_memoisation = thread_manager_init_memoisation_noop;
    tm->memoise          = thread_manager_memoise_noop;

    tm->impl = stm;

    return tm;
}

static void spencer_thread_manager_reset(void *impl)
{
    Thread           *t;
    SpencerScheduler *ss = ((SpencerThreadManager *) impl)->scheduler;

    ss->in_order_idx = 0;
    for (t = spencer_scheduler_next(ss); t; t = spencer_scheduler_next(ss))
        spencer_thread_manager_kill_thread(impl, t);
}

static void spencer_thread_manager_free(void *impl)
{
    spencer_thread_manager_reset(impl);
    spencer_scheduler_free(((SpencerThreadManager *) impl)->scheduler);
    free(impl);
}

static Thread *
spencer_thread_manager_spawn_thread(void *impl, const byte *pc, const char *sp)
{
    SpencerThread        *st   = malloc(sizeof(*st));
    SpencerThreadManager *self = impl;

    st->pc = pc;
    st->sp = sp;

    stc_slice_init(st->counters, self->ncounters);
    memset(st->counters, 0, self->ncounters * sizeof(*st->counters));
    stc_slice_init(st->memory, self->memory_len);
    memset(st->memory, 0, self->memory_len * sizeof(*st->memory));
    stc_slice_init(st->captures, 2 * self->ncaptures);
    memset(st->captures, 0, 2 * self->ncaptures * sizeof(*st->captures));

    return (Thread *) st;
}

static void spencer_thread_manager_schedule_thread(void *impl, Thread *t)
{
    spencer_scheduler_schedule(((SpencerThreadManager *) impl)->scheduler, t);
}

static void spencer_thread_manager_schedule_thread_in_order(void   *impl,
                                                            Thread *t)
{
    spencer_scheduler_schedule_in_order(
        ((SpencerThreadManager *) impl)->scheduler, t);
}

static Thread *spencer_thread_manager_next_thread(void *impl)
{
    return spencer_scheduler_next(((SpencerThreadManager *) impl)->scheduler);
}

static void spencer_thread_manager_notify_thread_match(void *impl, Thread *t)
{
    // empty the scheduler
    spencer_thread_manager_kill_thread(impl, t);
    spencer_thread_manager_reset(impl);
}

static Thread *spencer_thread_manager_clone_thread(void *impl, const Thread *t)
{
    SpencerThread *st = malloc(sizeof(*st));
    memcpy(st, t, sizeof(*st));

    UNUSED(impl);
    st->captures = stc_slice_clone(((SpencerThread *) t)->captures);
    st->counters = stc_slice_clone(((SpencerThread *) t)->counters);
    st->memory   = stc_slice_clone(((SpencerThread *) t)->memory);

    return (Thread *) st;
}

static void spencer_thread_manager_kill_thread(void *impl, Thread *t)
{
    UNUSED(impl);
    stc_slice_free(((SpencerThread *) t)->captures);
    stc_slice_free(((SpencerThread *) t)->counters);
    stc_slice_free(((SpencerThread *) t)->memory);
    free(t);
}

static const byte *spencer_thread_pc(void *impl, const Thread *t)
{
    UNUSED(impl);
    return t->pc;
}

static void spencer_thread_set_pc(void *impl, Thread *t, const byte *pc)
{
    UNUSED(impl);
    t->pc = pc;
}

static const char *spencer_thread_sp(void *impl, const Thread *t)
{
    UNUSED(impl);
    return t->sp;
}

static void spencer_thread_inc_sp(void *impl, Thread *t)
{
    UNUSED(impl);
    t->sp = stc_utf8_str_next(t->sp);
}

static cntr_t spencer_thread_counter(void *impl, const Thread *t, len_t idx)
{
    UNUSED(impl);
    return ((SpencerThread *) t)->counters[idx];
}

static void
spencer_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val)
{
    UNUSED(impl);
    ((SpencerThread *) t)->counters[idx] = val;
}

static void spencer_thread_inc_counter(void *impl, Thread *t, len_t idx)
{
    UNUSED(impl);
    ((SpencerThread *) t)->counters[idx]++;
}

static void *spencer_thread_memory(void *impl, const Thread *t, len_t idx)
{
    UNUSED(impl);
    return ((SpencerThread *) t)->memory + idx;
}

static void spencer_thread_set_memory(void       *self,
                                      Thread     *t,
                                      len_t       idx,
                                      const void *val,
                                      size_t      size)
{
    UNUSED(self);
    memcpy(((SpencerThread *) t)->memory + idx, val, size);
}

static const char *const *
spencer_thread_captures(void *impl, const Thread *t, len_t *ncaptures)
{
    UNUSED(impl);
    if (ncaptures)
        *ncaptures = stc_slice_len(((SpencerThread *) t)->captures) / 2;
    return ((SpencerThread *) t)->captures;
}

static void spencer_thread_set_capture(void *impl, Thread *t, len_t idx)
{
    UNUSED(impl);
    ((SpencerThread *) t)->captures[idx] = ((SpencerThread *) t)->sp;
}

/* --- SpencerScheduler function definitions -------------------------------- */

static SpencerScheduler *spencer_scheduler_new(void)
{
    SpencerScheduler *ss = malloc(sizeof(*ss));

    ss->in_order_idx = 0;
    ss->active       = NULL;
    stc_vec_default_init(ss->stack); // NOLINT(bugprone-sizeof-expression)

    return ss;
}

static void spencer_scheduler_schedule(SpencerScheduler *self, Thread *thread)
{
    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    if (self->active)
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    else
        self->active = thread;
}

static void spencer_scheduler_schedule_in_order(SpencerScheduler *self,
                                                Thread           *thread)
{
    size_t len = stc_vec_len_unsafe(self->stack);

    if (self->in_order_idx > len) {
        spencer_scheduler_schedule(self, thread);
        self->in_order_idx = len;
    } else if (self->in_order_idx == len) {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    } else {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_insert(self->stack, self->in_order_idx, thread);
    }
}

static Thread *spencer_scheduler_next(SpencerScheduler *self)
{
    SpencerThread *thread = (SpencerThread *) self->active;

    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    self->active       = NULL;
    if (thread == NULL && !stc_vec_is_empty(self->stack))
        thread = (SpencerThread *) stc_vec_pop(self->stack);

    return (Thread *) thread;
}

static void spencer_scheduler_free(SpencerScheduler *self)
{
    stc_vec_free(self->stack);
    free(self);
}
