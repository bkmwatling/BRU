#include <stdlib.h>

#include "thread_pool.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    ThreadPool *pool;
    void       *tm_impl;
} ThreadManagerWithPool;

/* --- API function definitions --------------------------------------------- */

ThreadManager *thread_manager_with_pool(ThreadManager *tm)
{
    ThreadManagerWithPool *tmwp = malloc(sizeof(*tmwp));
    tmwp->tm_impl               = tm;
    tmwp->pool                  = calloc(1, sizeof(*tmwp->pool));
    return tm;
}
