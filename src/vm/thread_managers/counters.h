#ifndef BRU_VM_THREAD_MANAGER_COUNTERS_H
#define BRU_VM_THREAD_MANAGER_COUNTERS_H

#include "thread_manager.h"

/**
 * Extend a thread manager with support for counter information.
 *
 * @param[in] tm        the underlying thread manager
 * @param[in] ncounters the number of captures
 *
 * @return the thread manager
 */
BruThreadManager *bru_thread_manager_with_counters_new(BruThreadManager *tm,
                                                       bru_len_t ncounters);

#endif /* BRU_VM_THREAD_MANAGER_COUNTERS_H */
