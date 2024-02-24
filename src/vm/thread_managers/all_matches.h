#ifndef ALL_MATCHES_H
#define ALL_MATCHES_H

#include <stdio.h>

#include "thread_manager.h"

/**
 * Track and report all matches by the underlying thread manager.
 *
 * @param[in] thread_manager the underlying thread manager
 * @param[in] logfile        the file for logging the matches
 *
 * @return the new thread manager
 */
ThreadManager *all_matches_thread_manager_new(ThreadManager *thread_manager,
                                              FILE          *logfile,
                                              const char    *text);

#endif
