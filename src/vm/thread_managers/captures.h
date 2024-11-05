#ifndef BRU_VM_THREAD_MANAGER_CAPTURES_H
#define BRU_VM_THREAD_MANAGER_CAPTURES_H

#include "thread_manager.h"

/**
 * Extend a thread manager with support for capture information.
 *
 * @param[in] tm        the underlying thread manager
 * @param[in] ncaptures the number of captures
 *
 * @return the thread manager
 */
BruThreadManager *bru_thread_manager_with_captures_new(BruThreadManager *tm,
                                                       bru_len_t ncaptures);

#endif /* BRU_VM_THREAD_MANAGER_CAPTURES_H */
