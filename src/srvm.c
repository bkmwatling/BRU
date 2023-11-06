#include "srvm.h"

static int srvm_run(const char *text, Scheduler *scheduler);

SRVM *srvm_new(Scheduler *scheduler);
int   srvm_match(SRVM *self, const char *text);
int   srvm_matches(Scheduler *scheduler, const char *text);
