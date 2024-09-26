#ifndef BRU_VM_THREAD_MANAGER_BITCODE_H
#define BRU_VM_THREAD_MANAGER_BITCODE_H

#include <stdio.h>

#include "thread_manager.h"

#if !defined(BRU_VM_THREAD_MANAGER_BITCODE_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_BITCODE_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                       \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                    \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define bitcode_thread_manager_new bru_bitcode_thread_manager_new
#endif /* BRU_VM_THREAD_MANAGER_BITCODE_ENABLE_SHORT_NAMES */

/**
 * Construct a thread manager that records bitcode information during matching
 * execution whilst using an underlying thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 * @param[in] logfile        the file stream for logging captures on match
 *
 * @return the constructed bitcode thread manager
 */
BruThreadManager *
bru_bitcode_thread_manager_new(BruThreadManager *thread_manager, FILE *logfile);

#endif /* BRU_VM_THREAD_MANAGER_BITCODE_H */
