#include <stdlib.h>
#include <string.h>

#include "../../stc/fatp/slice.h"
#include "../../stc/fatp/vec.h"

#include "../../utils.h"
#include "lockstep.h"
#include "thread_manager.h"
#include "thread_pool.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_thompson_thread {
    const bru_byte_t *pc;
    const char       *sp;
    bru_cntr_t       *counters; /**< stc_slice of counter values              */
    bru_byte_t       *memory;   /**< stc_slice for general memory             */
    const char      **captures; /**< stc_slice of capture SPs                 */
} BruThompsonThread;

typedef struct bru_thompson_scheduler {
    int in_lockstep; /**< whether to execute the synchronisation thread queue */

    BruThread **curr; /**< stc_vec for current queue of threads to execute    */
    BruThread **next; /**< stc_vec for next queue of threads to be executed   */
    BruThread **sync; /**< stc_vec for synchronisation queue for lockstep     */
} BruThompsonScheduler;

typedef struct bru_thompson_thread_manager {
    BruThompsonScheduler *scheduler; /**< Thompson scheduler for scheduling   */
    BruThreadPool        *pool;      /**< the pool of threads                 */
    const bru_byte_t     *start_pc;  /**< the starting PC for new threads     */
    const char           *sp;        /**< the string pointer for lockstep     */
    int                   matched;   /**< whether a match has been found      */

    // for spawning threads
    bru_len_t ncounters;  /**< number counter values to spawn threads with    */
    bru_len_t memory_len; /**< number bytes allocated for thread memory       */
    bru_len_t ncaptures;  /**< number captures to allocate memory in threads  */
} BruThompsonThreadManager;

/* --- ThompsonThreadManager function prototypes ---------------------------- */

static void thompson_thread_manager_init(void             *impl,
                                         const bru_byte_t *start_pc,
                                         const char       *start_sp);
static void thompson_thread_manager_reset(void *impl);
static void thompson_thread_manager_free(void *impl);
static int  thompson_thread_manager_done_exec(void *impl);

static void thompson_thread_manager_schedule_new_thread(void             *impl,
                                                        const bru_byte_t *pc,
                                                        const char       *sp);
static void thompson_thread_manager_schedule_thread(void *impl, BruThread *t);
#define thompson_thread_manager_schedule_thread_in_order \
    thompson_thread_manager_schedule_thread
static BruThread *thompson_thread_manager_next_thread(void *impl);
static void       thompson_thread_manager_notify_thread_match(void      *impl,
                                                              BruThread *t);
static BruThread *thompson_thread_manager_clone_thread(void            *impl,
                                                       const BruThread *t);
static void       thompson_thread_manager_kill_thread(void *impl, BruThread *t);

static const bru_byte_t *thompson_thread_pc(void *impl, const BruThread *t);
static void
thompson_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc);
static const char *thompson_thread_sp(void *impl, const BruThread *t);
static void        thompson_thread_inc_sp(void *impl, BruThread *t);
static bru_cntr_t
thompson_thread_counter(void *impl, const BruThread *t, bru_len_t idx);
static void thompson_thread_set_counter(void      *impl,
                                        BruThread *t,
                                        bru_len_t  idx,
                                        bru_cntr_t val);
static void
thompson_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx);
static void *
thompson_thread_memory(void *impl, const BruThread *t, bru_len_t idx);
static void thompson_thread_set_memory(void       *impl,
                                       BruThread  *t,
                                       bru_len_t   idx,
                                       const void *val,
                                       size_t      size);
static const char *const *
thompson_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures);
static void
           thompson_thread_set_capture(void *impl, BruThread *t, bru_len_t idx);
static int thompson_threads_contain(BruThread **threads, BruThread *thread);

/* --- ThompsonScheduler function prototypes -------------------------------- */

static BruThompsonScheduler *thompson_scheduler_new(void);
static int        thompson_scheduler_schedule(BruThompsonScheduler *self,
                                              BruThread            *thread);
static int        thompson_scheduler_done_step(BruThompsonScheduler *self);
static int        thompson_scheduler_has_next(BruThompsonScheduler *self);
static BruThread *thompson_scheduler_next(BruThompsonScheduler *self);
static void       thompson_scheduler_free(BruThompsonScheduler *self);

/* --- Helper function prototypes ------------------------------------------- */

static BruThompsonThread             *
thompson_thread_manager_new_thread(BruThompsonThreadManager *self);
static void thompson_thread_manager_copy_thread(BruThompsonThreadManager *self,
                                                BruThompsonThread        *dst,
                                                const BruThompsonThread  *src);
static BruThompsonThread             *
thompson_thread_manager_get_thread(BruThompsonThreadManager *self);
static void thompson_thread_free(BruThread *t);

/* --- ThompsonThreadManager function definitions --------------------------- */

BruThreadManager *bru_thompson_thread_manager_new(bru_len_t ncounters,
                                                  bru_len_t memory_len,
                                                  bru_len_t ncaptures,
                                                  FILE     *logfile)
{
    BruThreadManager         *tm  = malloc(sizeof(*tm));
    BruThompsonThreadManager *ttm = malloc(sizeof(*ttm));

    ttm->scheduler  = thompson_scheduler_new();
    ttm->pool       = bru_thread_pool_new(logfile);
    ttm->ncounters  = ncounters;
    ttm->memory_len = memory_len;
    ttm->ncaptures  = ncaptures;

    BRU_THREAD_MANAGER_SET_REQUIRED_FUNCS(tm, thompson);

    // TODO: create thread managers for below optional implementations
    tm->counter     = thompson_thread_counter;
    tm->set_counter = thompson_thread_set_counter;
    tm->inc_counter = thompson_thread_inc_counter;
    tm->memory      = thompson_thread_memory;
    tm->set_memory  = thompson_thread_set_memory;
    tm->captures    = thompson_thread_captures;
    tm->set_capture = thompson_thread_set_capture;

    // opt out of memoisation
    tm->init_memoisation = bru_thread_manager_init_memoisation_noop;
    tm->memoise          = bru_thread_manager_memoise_noop;

    tm->impl = ttm;

    return tm;
}

static void thompson_thread_manager_init(void             *impl,
                                         const bru_byte_t *start_pc,
                                         const char       *start_sp)
{
    BruThompsonThreadManager *self = impl;

    self->start_pc               = start_pc;
    self->sp                     = start_sp;
    self->matched                = FALSE;
    self->scheduler->in_lockstep = FALSE;

    thompson_thread_manager_schedule_new_thread(impl, start_pc, start_sp);
}

static void thompson_thread_manager_reset(void *impl)
{
    BruThread            *t;
    BruThompsonScheduler *ts = ((BruThompsonThreadManager *) impl)->scheduler;

    while ((t = thompson_scheduler_next(ts)))
        thompson_thread_manager_kill_thread(impl, t);
}

static void thompson_thread_manager_free(void *impl)
{
    BruThompsonThreadManager *self = impl;

    thompson_thread_manager_reset(impl);
    thompson_scheduler_free(self->scheduler);
    bru_thread_pool_free(self->pool, thompson_thread_free);
    free(impl);
}

static int thompson_thread_manager_done_exec(void *impl)
{
    return *((BruThompsonThreadManager *) impl)->sp == '\0';
}

static void thompson_thread_manager_schedule_new_thread(void             *impl,
                                                        const bru_byte_t *pc,
                                                        const char       *sp)
{
    BruThompsonThreadManager *self = impl;
    BruThompsonThread        *tt   = thompson_thread_manager_get_thread(self);

    tt->pc = pc;
    tt->sp = sp;

    memset(tt->counters, 0, sizeof(*tt->counters) * self->ncounters);
    memset(tt->memory, 0, sizeof(*tt->memory) * self->memory_len);
    memset(tt->captures, 0, sizeof(*tt->captures) * 2 * self->ncaptures);

    thompson_thread_manager_schedule_thread(impl, (BruThread *) tt);
}

static void thompson_thread_manager_schedule_thread(void *impl, BruThread *t)
{
    if (!thompson_scheduler_schedule(
            ((BruThompsonThreadManager *) impl)->scheduler, t))
        thompson_thread_manager_kill_thread(impl, t);
}

static BruThread *thompson_thread_manager_next_thread(void *impl)
{
    BruThompsonThreadManager *self = impl;
    size_t                    i, len;

    if (thompson_scheduler_done_step(self->scheduler) && *self->sp &&
        (!self->matched || thompson_scheduler_has_next(self->scheduler))) {
        self->sp = stc_utf8_str_next(self->sp);
        for (i = 0, len = stc_vec_len(self->scheduler->next); i < len; i++)
            self->scheduler->next[i]->sp = self->sp;
        for (i = 0, len = stc_vec_len(self->scheduler->sync); i < len; i++)
            self->scheduler->sync[i]->sp = self->sp;
        if (!self->matched)
            thompson_thread_manager_schedule_new_thread(impl, self->start_pc,
                                                        self->sp);
    }

    return thompson_scheduler_next(self->scheduler);
}

static void thompson_thread_manager_notify_thread_match(void      *impl,
                                                        BruThread *t)
{
    BruThompsonScheduler *ts = ((BruThompsonThreadManager *) impl)->scheduler;
    size_t                i, len = stc_vec_len(ts->curr);

    ((BruThompsonThreadManager *) impl)->matched = TRUE;
    thompson_thread_manager_kill_thread(impl, t);
    for (i = 0; i < len; i++)
        thompson_thread_manager_kill_thread(impl, ts->curr[i]);
    stc_vec_clear(ts->curr);
}

static BruThread *thompson_thread_manager_clone_thread(void            *impl,
                                                       const BruThread *t)
{
    BruThompsonThreadManager *self = impl;
    BruThompsonThread        *tt   = thompson_thread_manager_get_thread(self);

    thompson_thread_manager_copy_thread(self, tt, (BruThompsonThread *) t);

    return (BruThread *) tt;
}

static void thompson_thread_manager_kill_thread(void *impl, BruThread *t)
{
    BruThompsonThreadManager *self = impl;
    bru_thread_pool_add_thread(self->pool, t);
}

/* --- Thread function definitions ------------------------------------------ */

static const bru_byte_t *thompson_thread_pc(void *impl, const BruThread *t)
{
    BRU_UNUSED(impl);
    return t->pc;
}

static void
thompson_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc)
{
    BRU_UNUSED(impl);
    t->pc = pc;
}

static const char *thompson_thread_sp(void *impl, const BruThread *t)
{
    BRU_UNUSED(impl);
    return t->sp;
}

static void thompson_thread_inc_sp(void *impl, BruThread *t)
{
    BRU_UNUSED(impl);
    BRU_UNUSED(t);
    // t->sp = stc_utf8_str_next(t->sp);
}

static bru_cntr_t
thompson_thread_counter(void *impl, const BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    return ((BruThompsonThread *) t)->counters[idx];
}

static void thompson_thread_set_counter(void      *impl,
                                        BruThread *t,
                                        bru_len_t  idx,
                                        bru_cntr_t val)
{
    BRU_UNUSED(impl);
    ((BruThompsonThread *) t)->counters[idx] = val;
}

static void thompson_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    ((BruThompsonThread *) t)->counters[idx]++;
}

static void *
thompson_thread_memory(void *impl, const BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    return ((BruThompsonThread *) t)->memory + idx;
}

static void thompson_thread_set_memory(void       *impl,
                                       BruThread  *t,
                                       bru_len_t   idx,
                                       const void *val,
                                       size_t      size)
{
    BRU_UNUSED(impl);
    memcpy(((BruThompsonThread *) t)->memory + idx, val, size);
}

static const char *const *
thompson_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures)
{
    BRU_UNUSED(impl);
    if (ncaptures)
        *ncaptures = stc_slice_len(((BruThompsonThread *) t)->captures) / 2;
    return ((BruThompsonThread *) t)->captures;
}

static void thompson_thread_set_capture(void *impl, BruThread *t, bru_len_t idx)
{
    BRU_UNUSED(impl);
    ((BruThompsonThread *) t)->captures[idx] = t->sp;
}

static int thompson_thread_eq(BruThread *t1, BruThread *t2)
{
    BruThompsonThread *tt1 = (BruThompsonThread *) t1,
                      *tt2 = (BruThompsonThread *) t2;
    bru_len_t i, len = stc_slice_len(tt1->counters);
    int       eq = tt1->pc == tt2->pc && len == stc_slice_len(tt2->counters);

    for (i = 0; i < len && eq; i++) eq = tt1->counters[i] == tt2->counters[i];

    return eq;
}

static int thompson_threads_contain(BruThread **threads, BruThread *thread)
{
    size_t i, len;

    len = stc_vec_len(threads);
    for (i = 0; i < len; i++)
        if (thompson_thread_eq(threads[i], thread)) return TRUE;

    return FALSE;
}

/* --- ThompsonScheduler function definitions ------------------------------- */

static BruThompsonScheduler *thompson_scheduler_new(void)
{
    BruThompsonScheduler *ts = malloc(sizeof(*ts));

    ts->in_lockstep = FALSE;
    stc_vec_default_init(ts->curr); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(ts->next); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(ts->sync); // NOLINT(bugprone-sizeof-expression)

    return ts;
}

static int thompson_scheduler_schedule(BruThompsonScheduler *self,
                                       BruThread            *thread)
{
    if (thompson_threads_contain(self->sync, thread)) return FALSE;

    switch (*thread->pc) {
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

static int thompson_scheduler_done_step(BruThompsonScheduler *self)
{
    return stc_vec_is_empty(self->curr) && self->in_lockstep;
}

static int thompson_scheduler_has_next(BruThompsonScheduler *self)
{
    return !(stc_vec_is_empty(self->curr) && stc_vec_is_empty(self->next) &&
             stc_vec_is_empty(self->sync));
}

static BruThread *thompson_scheduler_next(BruThompsonScheduler *self)
{
    BruThread  *thread = NULL;
    BruThread **tmp;

thompson_scheduler_next_start:
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
        switch (*thread->pc) {
            case BRU_CHAR:
            case BRU_PRED:
                if (!self->in_lockstep) {
                    thompson_scheduler_schedule(self, thread);
                    goto thompson_scheduler_next_start;
                }
                break;

            default: break;
        }
    }

    return thread;
}

static void thompson_scheduler_free(BruThompsonScheduler *self)
{
    stc_vec_free(self->curr);
    stc_vec_free(self->next);
    stc_vec_free(self->sync);
    free(self);
}

/* --- Helper functions ----------------------------------------------------- */

static BruThompsonThread *
thompson_thread_manager_new_thread(BruThompsonThreadManager *self)
{
    BruThompsonThread *tt = malloc(sizeof(*tt));

    stc_slice_init(tt->memory, self->memory_len);
    stc_slice_init(tt->captures, 2 * self->ncaptures);
    stc_slice_init(tt->counters, self->ncounters);

    return tt;
}

static void thompson_thread_manager_copy_thread(BruThompsonThreadManager *self,
                                                BruThompsonThread        *dst,
                                                const BruThompsonThread  *src)
{
    dst->pc = src->pc;
    dst->sp = src->sp;
    memcpy(dst->memory, src->memory, sizeof(*dst->memory) * self->memory_len);
    memcpy(dst->captures, src->captures,
           sizeof(*dst->captures) * 2 * self->ncaptures);
    memcpy(dst->counters, src->counters,
           sizeof(*dst->counters) * self->ncounters);
}

static BruThompsonThread *
thompson_thread_manager_get_thread(BruThompsonThreadManager *self)
{
    BruThompsonThread *tt =
        (BruThompsonThread *) bru_thread_pool_get_thread(self->pool);

    if (!tt) tt = thompson_thread_manager_new_thread(self);

    return tt;
}

static void thompson_thread_free(BruThread *t)
{
    BruThompsonThread *tt = (BruThompsonThread *) t;

    stc_slice_free(tt->memory);
    stc_slice_free(tt->counters);
    stc_slice_free(tt->captures);
    free(tt);
}
