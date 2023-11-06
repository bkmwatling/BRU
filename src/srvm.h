#ifndef SRVM_H
#define SRVM_H

#include "scheduler.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    ThreadManager *thread_manager;
    Scheduler     *scheduler;
    len_t          ncaptures;
    len_t         *captures;
} SRVM;

/* --- SRVM function prototypes --------------------------------------------- */

SRVM *srvm_new(Scheduler *scheduler);
int   srvm_match(SRVM *self, const char *text);
int   srvm_matches(Scheduler *scheduler, const char *text);

#endif /* SRVM_H */
