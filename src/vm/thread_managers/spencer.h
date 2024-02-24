#ifndef SPENCER_H
#define SPENCER_H

#include <stddef.h>

#include "../../types.h"
#include "../program.h"

#include "thread_manager.h"

/* --- SpencerThread function prototypes ------------------------------------ */

ThreadManager *
spencer_thread_manager_new(len_t ncounters, len_t memory_len, len_t ncaptures);

#endif /* SPENCER_H */
