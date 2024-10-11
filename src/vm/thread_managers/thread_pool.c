#include <stdlib.h>

#include "thread_pool.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_thread_list BruThreadList;

struct bru_thread_list {
    BruThread     *thread;
    BruThreadList *next;
};

typedef struct {
    BruThreadList *pool;
    FILE          *logfile;
    int            enabled; /**< if the pool is enabled; see free/kill_thread */
} BruThreadPoolThreadManager;

/* --- ThreadPool funtion prototypes ---------------------------------------- */

static void thread_pool_free(BruThreadManager *tm);

static BruThread *thread_pool_alloc_thread(BruThreadManager *tm);
static void       thread_pool_kill_thread(BruThreadManager *tm, BruThread *t);
static BruThread *thread_pool_clone_thread(BruThreadManager *tm,
                                           const BruThread  *t);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_thread_manager_with_pool_new(BruThreadManager *tm,
                                                   FILE             *logfile)
{
    BruThreadPoolThreadManager *pool = malloc(sizeof(*pool));
    BruThreadManagerInterface  *tmi, *super;

    super = bru_vt_curr(tm);
    tmi   = bru_thread_manager_interface_new(pool, super->_thread_size);
    tmi->alloc_thread = thread_pool_alloc_thread;
    tmi->clone_thread = thread_pool_clone_thread;
    tmi->kill_thread  = thread_pool_kill_thread;
    tmi->free         = thread_pool_free;

    pool->pool    = NULL;
    pool->enabled = TRUE;
    pool->logfile = logfile;

    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- Helper functions ----------------------------------------------------- */

static void thread_pool_free(BruThreadManager *tm)
{
    BruThreadPoolThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);
#ifdef BRU_BENCHMARK
    size_t nthreads = 0;
#endif /* BRU_BENCHMARK */
    BruThreadList *p;

    self->enabled = FALSE;
    while ((p = self->pool)) {
        self->pool = p->next;
#ifdef BRU_BENCHMARK
        nthreads++;
#endif /* BRU_BENCHMARK */
        bru_thread_manager_kill_thread(tm, p->thread);
        free(p);
    }

#ifdef BRU_BENCHMARK
    fprintf(self->logfile, "TOTAL THREADS IN POOL: %zu\n", nthreads);
#endif /* BRU_BENCHMARK */

    bru_vt_shrink(tm);
    bru_vt_call_procedure(tm, free);

    free(self);
    bru_thread_manager_interface_free(tmi);
}

static BruThread *bru_thread_pool_get_thread(BruThreadPoolThreadManager *self)
{
    BruThread     *t;
    BruThreadList *p;

    if ((p = self->pool)) {
        self->pool = p->next;
        t          = p->thread;
        free(p);
    } else {
        t = NULL;
    }

    return t;
}

/* --- ThreadPool manager functions ----------------------------------------- */

static BruThread *thread_pool_alloc_thread(BruThreadManager *tm)
{
    BruThreadPoolThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);
    BruThread                  *thread;

    if (!(thread = bru_thread_pool_get_thread(self)))
        bru_vt_call_super_function(tm, tmi, thread, alloc_thread);

    return thread;
}

static BruThread *thread_pool_clone_thread(BruThreadManager *tm,
                                           const BruThread  *t)
{
    BruThreadPoolThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);
    BruThread                  *clone;

    if ((clone = bru_thread_pool_get_thread(self)))
        bru_thread_manager_copy_thread(tm, t, clone);
    else
        bru_vt_call_super_function(tm, tmi, clone, clone_thread, t);

    return clone;
}

static void thread_pool_kill_thread(BruThreadManager *tm, BruThread *t)
{
    BruThreadPoolThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);
    BruThreadList              *p;

    if (!self->enabled) {
        bru_vt_call_super_procedure(tm, tmi, kill_thread, t);
    } else if (t) {
        p          = malloc(sizeof(*p));
        p->thread  = t;
        p->next    = self->pool;
        self->pool = p;
    }
}
