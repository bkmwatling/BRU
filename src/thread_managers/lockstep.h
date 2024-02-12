#ifndef LOCKSTEP_H
#define LOCKSTEP_H

#include <stddef.h>

#include "../program.h"
#include "../types.h"
#include "thread_manager.h"

/* --- Thread function prototypes ----------------------------------- */

ThreadManager *
thompson_thread_manager_new(len_t ncounters, len_t memory_len, len_t ncaptures);

#endif
