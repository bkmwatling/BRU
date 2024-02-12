#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "thread_manager.h"

typedef struct thread_pool {
    Thread             *thread;
    struct thread_pool *next;
} ThreadPool;

#endif
