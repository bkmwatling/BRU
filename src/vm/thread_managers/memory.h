#ifndef BRU_VM_THREAD_MANAGER_MEMORY_H
#define BRU_VM_THREAD_MANAGER_MEMORY_H

#include "thread_manager.h"

/**
 * Extend a thread manager with support for memory information.
 *
 * @param[in] tm         the underlying thread manager
 * @param[in] memory_len the length of the memory in bytes
 *
 * @return the thread manager
 */
BruThreadManager *bru_thread_manager_with_memory_new(BruThreadManager *tm,
                                                     bru_len_t memory_len);

#endif /* BRU_VM_THREAD_MANAGER_MEMORY_H */
