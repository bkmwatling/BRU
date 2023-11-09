#ifndef LOCKSTEP_H
#define LOCKSTEP_H

#include <stddef.h>

#include "program.h"
#include "scheduler.h"
#include "types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct thompson_thread    ThompsonThread;
typedef struct thompson_scheduler ThompsonScheduler;

/* --- ThompsonThread function prototypes ----------------------------------- */

ThompsonThread    *thompson_thread_new(const byte        *pc,
                                       const char *const *sp,
                                       len_t              ncaptures,
                                       const cntr_t      *counters,
                                       len_t              counters_len,
                                       const byte        *memory,
                                       len_t              memory_len);
const byte        *thompson_thread_pc(const ThompsonThread *self);
void               thompson_thread_set_pc(ThompsonThread *self, const byte *pc);
const char        *thompson_thread_sp(const ThompsonThread *self);
void               thompson_thread_try_inc_sp(ThompsonThread *self);
const char *const *thompson_thread_captures(const ThompsonThread *self,
                                            len_t                *ncaptures);
void               thompson_thread_set_capture(ThompsonThread *self, len_t idx);
cntr_t thompson_thread_counter(const ThompsonThread *self, len_t idx);
void   thompson_thread_set_counter(ThompsonThread *self, len_t idx, cntr_t val);
void   thompson_thread_inc_counter(ThompsonThread *self, len_t idx);
void  *thompson_thread_memory(const ThompsonThread *self, len_t idx);
void   thompson_thread_set_memory(ThompsonThread *self,
                                  len_t           idx,
                                  const void     *val,
                                  size_t          size);
ThompsonThread *thompson_thread_clone(const ThompsonThread *self);
void            thompson_thread_free(ThompsonThread *self);

ThreadManager *thompson_thread_manager_new(void);

/* --- ThompsonScheduler function prototypes -------------------------------- */

ThompsonScheduler *thompson_scheduler_new(const Program *program,
                                          const char    *text);
void thompson_scheduler_init(ThompsonScheduler *self, const char *text);
void thompson_scheduler_schedule(ThompsonScheduler *self,
                                 ThompsonThread    *thread);
int  thompson_scheduler_has_next(const ThompsonScheduler *self);
ThompsonThread *thompson_scheduler_next(ThompsonScheduler *self);
void            thompson_scheduler_reset(ThompsonScheduler *self);
void            thompson_scheduler_notify_match(ThompsonScheduler *self);
void thompson_scheduler_kill(ThompsonScheduler *self, ThompsonThread *thread);
ThompsonScheduler *thompson_scheduler_clone(const ThompsonScheduler *self);
ThompsonScheduler *thompson_scheduler_clone_with(const ThompsonScheduler *self,
                                                 ThompsonThread *thread);
const Program     *thompson_scheduler_program(const ThompsonScheduler *self);
void               thompson_scheduler_free(ThompsonScheduler *self);

Scheduler *thompson_scheduler_as_scheduler(ThompsonScheduler *self);
Scheduler *thompson_scheduler_to_scheduler(ThompsonScheduler *self);

#endif /* LOCKSTEP_H */
