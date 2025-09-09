#ifndef BRU_VM_THREAD_MANAGER_BACKTRACK_H
#define BRU_VM_THREAD_MANAGER_BACKTRACK_H

#include <bru/vm/thread/managers/manager.h>

#if !defined(BRU_VM_THREAD_MANAGER_BACKTRACK_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_THREAD_MANAGER_BACKTRACK_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&                         \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||                      \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define backtrack_tm_new bru_backtrack_tm_new
#endif /* BRU_VM_THREAD_MANAGER_BACKTRACK_ENABLE_SHORT_NAMES */

/* --- Backtrack ThreadManager function prototypes -------------------------- */

/**
 * Construct a thread manager that performs Spencer-style backtracking regex
 * matching.
 *
 * @return the constructed Spencer-style thread manager
 */
BruThreadManager *bru_backtrack_tm_new(void);

#endif /* BRU_VM_THREAD_MANAGER_BACKTRACK_H */
