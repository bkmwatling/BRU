#include <stdlib.h>

#include "thread_pool.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_thread_list BruThreadList;

struct bru_thread_list {
    BruThread     *thread;
    BruThreadList *next;
};

struct bru_thread_pool {
    BruThreadList *pool;
    FILE          *logfile;
};

/* --- API function definitions --------------------------------------------- */

BruThreadPool *bru_thread_pool_new(FILE *logfile)
{
    BruThreadPool *pool = malloc(sizeof(*pool));

    pool->pool    = NULL;
    pool->logfile = logfile;

    return pool;
}

void bru_thread_pool_free(BruThreadPool *self,
                          void           (*thread_free)(BruThread *t))
{
#ifdef BRU_BENCHMARK
    size_t nthreads = 0;
#endif /* BRU_BENCHMARK */
    BruThreadList *p;

    while ((p = self->pool)) {
        self->pool = p->next;
#ifdef BRU_BENCHMARK
        nthreads++;
#endif /* BRU_BENCHMARK */
        thread_free(p->thread);
        free(p);
    }

#ifdef BRU_BENCHMARK
    fprintf(self->logfile, "TOTAL THREADS IN POOL: %zu\n", nthreads);
#endif /* BRU_BENCHMARK */

    free(self);
}

BruThread *bru_thread_pool_get_thread(BruThreadPool *self)
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

void bru_thread_pool_add_thread(BruThreadPool *self, BruThread *t)
{
    BruThreadList *p;

    if (t) {
        p          = malloc(sizeof(*p));
        p->thread  = t;
        p->next    = self->pool;
        self->pool = p;
    }
}
