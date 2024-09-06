#include <stdlib.h>
#include <string.h>

#include "../../stc/fatp/slice.h"
#include "../../stc/fatp/vec.h"

#include "../../utils.h"
#include "spencer.h"
#include "thread_manager.h"
#include "thread_pool.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_spencer_thread {
    const bru_byte_t *pc;
    const char       *sp;
    bru_cntr_t       *counters; /**< stc_slice of counter values              */
    bru_byte_t       *memory;   /**< stc_slice for general memory             */
    const char      **captures; /**< stc_slice of capture SPs                 */
} BruSpencerThread;

typedef struct bru_spencer_scheduler {
    size_t in_order_idx; /**< the index for inserting threads when in-order   */
    BruThread  *active;  /**< the active thread for the scheduler             */
    BruThread **stack;   /**< stc_vec for thread stack for DFS scheduling     */
} BruSpencerScheduler;

typedef struct bru_spencer_thread_manager {
    BruSpencerScheduler *scheduler; /**< the Spencer scheduler for scheduling */
    BruThreadPool       *pool;      /**< the pool of threads                  */
    const char          *sp;        /**< the string pointer for the manager   */

    // for spawning threads
    bru_len_t ncounters;  /**< number of counter values to spawn threads with */
    bru_len_t memory_len; /**< number bytes allocated for thread memory       */
    bru_len_t ncaptures;  /**< number captures to allocate memory in threads  */
} BruSpencerThreadManager;

/* --- SpencerThreadManager function prototypes ----------------------------- */

static void spencer_thread_manager_init(void             *impl,
                                        const bru_byte_t *start_pc,
                                        const char       *start_sp);
static void spencer_thread_manager_reset(void *impl);
static void spencer_thread_manager_free(void *impl);
static int  spencer_thread_manager_done_exec(void *impl);

static void spencer_thread_manager_schedule_thread(void *impl, BruThread *t);
static void spencer_thread_manager_schedule_thread_in_order(void      *impl,
                                                            BruThread *t);
static BruThread *spencer_thread_manager_next_thread(void *impl);
static void       spencer_thread_manager_notify_thread_match(void      *impl,
                                                             BruThread *t);
static BruThread *spencer_thread_manager_clone_thread(void            *impl,
                                                      const BruThread *t);
static void       spencer_thread_manager_kill_thread(void *impl, BruThread *t);
static const bru_byte_t *spencer_thread_pc(void *impl, const BruThread *t);
static void
spencer_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc);
static const char *spencer_thread_sp(void *impl, const BruThread *t);
static void        spencer_thread_inc_sp(void *impl, BruThread *t);
static bru_cntr_t
spencer_thread_counter(void *impl, const BruThread *t, bru_len_t idx);
static void spencer_thread_set_counter(void      *impl,
                                       BruThread *t,
                                       bru_len_t  idx,
                                       bru_cntr_t val);
static void spencer_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx);
static void *
spencer_thread_memory(void *impl, const BruThread *t, bru_len_t idx);
static void spencer_thread_set_memory(void       *impl,
                                      BruThread  *t,
                                      bru_len_t   idx,
                                      const void *val,
                                      size_t      size);
static const char *const *
spencer_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures);
static void spencer_thread_set_capture(void *impl, BruThread *t, bru_len_t idx);

/* --- Scheduler function prototypes ---------------------------------------- */

static BruSpencerScheduler *spencer_scheduler_new(void);
static void       spencer_scheduler_schedule(BruSpencerScheduler *self,
                                             BruThread           *thread);
static void       spencer_scheduler_schedule_in_order(BruSpencerScheduler *self,
                                                      BruThread           *thread);
static BruThread *spencer_scheduler_next(BruSpencerScheduler *self);
static void       spencer_scheduler_free(BruSpencerScheduler *self);

/* --- Helper function prototypes ------------------------------------------- */

static BruSpencerThread             *
spencer_thread_manager_new_thread(BruSpencerThreadManager *self);
static void spencer_thread_manager_copy_thread(BruSpencerThreadManager *self,
                                               BruSpencerThread        *dst,
                                               const BruSpencerThread  *src);
static BruSpencerThread             *
spencer_thread_manager_get_thread(BruSpencerThreadManager *self);
static void spencer_thread_free(BruThread *t);

/* --- SpencerThreadManager function definitions ---------------------------- */

BruThreadManager *bru_spencer_thread_manager_new(bru_len_t ncounters,
                                                 bru_len_t memory_len,
                                                 bru_len_t ncaptures,
                                                 FILE     *logfile)
{
    BruThreadManager        *tm  = malloc(sizeof(*tm));
    BruSpencerThreadManager *stm = malloc(sizeof(*stm));

    stm->scheduler  = spencer_scheduler_new();
    stm->pool       = bru_thread_pool_new(logfile);
    stm->ncounters  = ncounters;
    stm->memory_len = memory_len;
    stm->ncaptures  = ncaptures;

    BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(tm, spencer);

    // TODO: create thread managers for below optional implementations
    tm->counter     = spencer_thread_counter;
    tm->set_counter = spencer_thread_set_counter;
    tm->inc_counter = spencer_thread_inc_counter;
    tm->memory      = spencer_thread_memory;
    tm->set_memory  = spencer_thread_set_memory;
    tm->captures    = spencer_thread_captures;
    tm->set_capture = spencer_thread_set_capture;

    // opt out of memoisation
    tm->init_memoisation = bru_thread_manager_init_memoisation_noop;
    tm->memoise          = bru_thread_manager_memoise_noop;

    tm->impl = stm;

    return tm;
}

static void spencer_thread_manager_init(void             *impl,
                                        const bru_byte_t *start_pc,
                                        const char       *start_sp)
{
    BruSpencerThreadManager *self = impl;
    BruSpencerThread        *st   = spencer_thread_manager_get_thread(self);

    self->sp = start_sp;

    st->pc = start_pc;
    st->sp = start_sp;

    memset(st->counters, 0, sizeof(*st->counters) * self->ncounters);
    memset(st->memory, 0, sizeof(*st->memory) * self->memory_len);
    memset(st->captures, 0, sizeof(*st->captures) * 2 * self->ncaptures);

    spencer_thread_manager_schedule_thread(impl, (BruThread *) st);
}

static void spencer_thread_manager_reset(void *impl)
{
    BruThread           *t;
    BruSpencerScheduler *ss = ((BruSpencerThreadManager *) impl)->scheduler;

    ss->in_order_idx = 0;
    while ((t = spencer_scheduler_next(ss)))
        spencer_thread_manager_kill_thread(impl, t);
}

static void spencer_thread_manager_free(void *impl)
{
    BruSpencerThreadManager *self = impl;

    spencer_thread_manager_reset(impl);
    spencer_scheduler_free(self->scheduler);
    bru_thread_pool_free(self->pool, spencer_thread_free);
    free(impl);
}

static int spencer_thread_manager_done_exec(void *impl)
{
    return *((BruSpencerThreadManager *) impl)->sp == '\0';
}

static void spencer_thread_manager_schedule_thread(void *impl, BruThread *t)
{
    spencer_scheduler_schedule(((BruSpencerThreadManager *) impl)->scheduler,
                               t);
}

static void spencer_thread_manager_schedule_thread_in_order(void      *impl,
                                                            BruThread *t)
{
    spencer_scheduler_schedule_in_order(
        ((BruSpencerThreadManager *) impl)->scheduler, t);
}

static BruThread *spencer_thread_manager_next_thread(void *impl)
{
    return spencer_scheduler_next(
        ((BruSpencerThreadManager *) impl)->scheduler);
}

static void spencer_thread_manager_notify_thread_match(void *impl, BruThread *t)
{
    // empty the scheduler
    spencer_thread_manager_kill_thread(impl, t);
    spencer_thread_manager_reset(impl);
}

static BruThread *spencer_thread_manager_clone_thread(void            *impl,
                                                      const BruThread *t)
{
    BruSpencerThreadManager *self = impl;
    BruSpencerThread        *st   = spencer_thread_manager_get_thread(self);

    spencer_thread_manager_copy_thread(self, st, (BruSpencerThread *) t);

    return (BruThread *) st;
}

static void spencer_thread_manager_kill_thread(void *impl, BruThread *t)
{
    bru_thread_pool_add_thread(((BruSpencerThreadManager *) impl)->pool, t);
}

static const bru_byte_t *spencer_thread_pc(void *impl, const BruThread *t)
{
    BRU_UNUSED(impl);
    return t->pc;
}

static void
spencer_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc)
{
    BRU_UNUSED(impl);
    t->pc = pc;
}

static const char *spencer_thread_sp(void *impl, const BruThread *t)
{
    BRU_UNUSED(impl);
    return t->sp;
}

static void spencer_thread_inc_sp(void *impl, BruThread *t)
{
    BRU_UNUSED(impl);
    t->sp = stc_utf8_str_next(t->sp);
}

static bru_cntr_t
spencer_thread_counter(void *impl, const BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    return ((BruSpencerThread *) t)->counters[idx];
}

static void spencer_thread_set_counter(void      *impl,
                                       BruThread *t,
                                       bru_len_t  idx,
                                       bru_cntr_t val)
{
    BRU_UNUSED(impl);
    ((BruSpencerThread *) t)->counters[idx] = val;
}

static void spencer_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    ((BruSpencerThread *) t)->counters[idx]++;
}

static void *
spencer_thread_memory(void *impl, const BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    return ((BruSpencerThread *) t)->memory + idx;
}

static void spencer_thread_set_memory(void       *self,
                                      BruThread  *t,
                                      bru_len_t   idx,
                                      const void *val,
                                      size_t      size)
{
    BRU_UNUSED(self);
    memcpy(((BruSpencerThread *) t)->memory + idx, val, size);
}

static const char *const *
spencer_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures)
{
    BRU_UNUSED(impl);
    if (ncaptures)
        *ncaptures = stc_slice_len(((BruSpencerThread *) t)->captures) / 2;
    return ((BruSpencerThread *) t)->captures;
}

static void spencer_thread_set_capture(void *impl, BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    ((BruSpencerThread *) t)->captures[idx] = ((BruSpencerThread *) t)->sp;
}

/* --- SpencerScheduler function definitions -------------------------------- */

static BruSpencerScheduler *spencer_scheduler_new(void)
{
    BruSpencerScheduler *ss = malloc(sizeof(*ss));

    ss->in_order_idx = 0;
    ss->active       = NULL;
    stc_vec_default_init(ss->stack); // NOLINT(bugprone-sizeof-expression)

    return ss;
}

static void spencer_scheduler_schedule(BruSpencerScheduler *self,
                                       BruThread           *thread)
{
    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    if (self->active)
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    else
        self->active = thread;
}

static void spencer_scheduler_schedule_in_order(BruSpencerScheduler *self,
                                                BruThread           *thread)
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

static BruThread *spencer_scheduler_next(BruSpencerScheduler *self)
{
    BruSpencerThread *thread = (BruSpencerThread *) self->active;

    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    self->active       = NULL;
    if (thread == NULL && !stc_vec_is_empty(self->stack))
        thread = (BruSpencerThread *) stc_vec_pop(self->stack);

    return (BruThread *) thread;
}

static void spencer_scheduler_free(BruSpencerScheduler *self)
{
    stc_vec_free(self->stack);
    free(self);
}

/* --- Helper functions ----------------------------------------------------- */

static BruSpencerThread *
spencer_thread_manager_new_thread(BruSpencerThreadManager *self)
{
    BruSpencerThread *st = malloc(sizeof(*st));

    stc_slice_init(st->memory, self->memory_len);
    stc_slice_init(st->captures, 2 * self->ncaptures);
    stc_slice_init(st->counters, self->ncounters);

    return st;
}

static void spencer_thread_manager_copy_thread(BruSpencerThreadManager *self,
                                               BruSpencerThread        *dst,
                                               const BruSpencerThread  *src)
{
    dst->pc = src->pc;
    dst->sp = src->sp;
    memcpy(dst->memory, src->memory, sizeof(*dst->memory) * self->memory_len);
    memcpy(dst->captures, src->captures,
           sizeof(*dst->captures) * 2 * self->ncaptures);
    memcpy(dst->counters, src->counters,
           sizeof(*dst->counters) * self->ncounters);
}

static BruSpencerThread *
spencer_thread_manager_get_thread(BruSpencerThreadManager *self)
{
    BruSpencerThread *st =
        (BruSpencerThread *) bru_thread_pool_get_thread(self->pool);

    if (!st) st = spencer_thread_manager_new_thread(self);

    return st;
}

static void spencer_thread_free(BruThread *t)
{
    BruSpencerThread *st = (BruSpencerThread *) t;

    stc_slice_free(st->memory);
    stc_slice_free(st->counters);
    stc_slice_free(st->captures);
    free(st);
}
