#ifndef BRU_VM_THREAD_MANAGER_ALL_MATCHES_H
#define BRU_VM_THREAD_MANAGER_ALL_MATCHES_H

#include <stdio.h>

#include "thread_manager.h"

#if !defined(BRU_VM_THREAD_MANAGER_ALL_MATCHES_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_ALL_MATCHES_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                           \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                        \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define all_matches_thread_manager_new bru_all_matches_thread_manager_new
#endif /* BRU_VM_THREAD_MANAGER_ALL_MATCHES_ENABLE_SHORT_NAMES */

/**
 * Construct a thread manager that tracks and reports all matches by the
 * underlying thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 * @param[in] logfile        the file stream for logging captures on match
 * @param[in] text           the input string being matched against
 *
 * @return the constructed all matches thread manager
 */
BruThreadManager *
bru_all_matches_thread_manager_new(BruThreadManager *thread_manager,
                                   FILE             *logfile,
                                   const char       *text);

#endif /* BRU_VM_THREAD_MANAGER_ALL_MATCHES_H */
