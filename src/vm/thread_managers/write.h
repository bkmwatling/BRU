#ifndef BRU_VM_THREAD_MANAGER_WRITE_H
#define BRU_VM_THREAD_MANAGER_WRITE_H

#include "thread_manager.h"

/**
 * Extend a thread manager with support for writing arbitrary bytes.
 *
 * @param[in] tm        the underlying thread manager
 *
 * @return the thread manager
 */
BruThreadManager *bru_thread_manager_with_write_new(BruThreadManager *tm);

#endif /* BRU_VM_THREAD_MANAGER_WRITE_H */
