#ifndef BRU_VM_THREAD_MANAGER_CAPTURES_H
#define BRU_VM_THREAD_MANAGER_CAPTURES_H

#include <bru/vm/thread/managers/manager.h>

/**
 * Extend a thread manager with support for back references.
 *
 * @param[in] tm the underlying thread manager
 *
 * @return the thread manager
 */
BruThreadManager *bru_tm_with_backrefs_new(BruThreadManager *tm);

#endif /* BRU_VM_THREAD_MANAGER_CAPTURES_H */
