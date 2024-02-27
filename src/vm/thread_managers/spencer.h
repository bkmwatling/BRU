#ifndef SPENCER_THREAD_MANAGER_H
#define SPENCER_THREAD_MANAGER_H

#include <stddef.h>

#include "../../types.h"
#include "../program.h"
#include "thread_manager.h"

/* --- Spencer ThreadManager function prototypes ---------------------------- */

/**
 * Construct a thread manager that performs Spencer-style regex matching.
 *
 * @param[in] ncounters  the number of counters needed
 * @param[in] memory_len the number of bytes to allocate for thread memory
 * @param[in] ncaptures  the number of captures needed
 *
 * @return the constructed Spencer-style thread manager
 */
ThreadManager *
spencer_thread_manager_new(len_t ncounters, len_t memory_len, len_t ncaptures);

#endif /* SPENCER_THREAD_MANAGER_H */
