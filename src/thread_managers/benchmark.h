#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <stdio.h>

#include "thread_manager.h"

ThreadManager *benchmark_thread_manager_new(ThreadManager *thread_manager,
                                            FILE          *logfile);

#endif
