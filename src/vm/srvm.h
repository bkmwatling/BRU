#ifndef SRVM_H
#define SRVM_H

#include "../stc/fatp/string_view.h"

#include "thread_managers/thread_manager.h"

/* --- Preprocessor constants ----------------------------------------------- */

#define NO_MATCH          0
#define MATCH             1
#define MATCHES_EXHAUSTED 2

/* --- Type definitions ----------------------------------------------------- */

typedef struct srvm SRVM;

/* --- SRVM function prototypes --------------------------------------------- */

SRVM          *srvm_new(ThreadManager *thread_manager, const Program *prog);
void           srvm_free(SRVM *self);
int            srvm_match(SRVM *self, const char *text);
int            srvm_find(SRVM *self, const char *text);
StcStringView  srvm_capture(SRVM *self, len_t idx);
StcStringView *srvm_captures(SRVM *self, len_t *ncaptures);
int            srvm_matches(ThreadManager *thread_manager,
                            const Program *prog,
                            const char    *text);

#endif /* SRVM_H */
