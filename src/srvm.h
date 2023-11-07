#ifndef SRVM_H
#define SRVM_H

#include "scheduler.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct srvm SRVM;

/* --- SRVM function prototypes --------------------------------------------- */

SRVM  *srvm_new(ThreadManager *thread_manager, Scheduler *scheduler);
void   srvm_free(SRVM *self);
int    srvm_match(SRVM *self, const char *text);
void   srvm_capture(SRVM *self, len_t idx, len_t *start, len_t *end);
len_t *srvm_captures(SRVM *self, len_t *ncaptures);
int    srvm_matches(ThreadManager *thread_manager,
                    Scheduler *scheduler,
                    const char *text);

#endif /* SRVM_H */
