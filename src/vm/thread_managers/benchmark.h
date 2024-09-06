#ifndef BRU_VM_THREAD_MANAGER_BENCHMARK_H
#define BRU_VM_THREAD_MANAGER_BENCHMARK_H

#include <stdio.h>

#include "thread_manager.h"

#if !defined(BRU_VM_THREAD_MANAGER_BENCHMARK_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_BENCHMARK_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                         \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                      \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define benchmark_thread_manager_new bru_benchmark_thread_manager_new
#endif /* BRU_VM_THREAD_MANAGER_BENCHMARK_ENABLE_SHORT_NAMES */

/**
 * Construct a thread manager that benchmarks the performance of an SRVM
 * execution whilst using an underlying thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 * @param[in] logfile        the file stream for logging captures on match
 *
 * @return the constructed benchmark thread manager
 */
BruThreadManager *
bru_benchmark_thread_manager_new(BruThreadManager *thread_manager,
                                 FILE             *logfile);

#endif /* BRU_VM_THREAD_MANAGER_BENCHMARK_H */
