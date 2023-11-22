#ifndef SPENCER_H
#define SPENCER_H

#include <stddef.h>

#include "program.h"
#include "scheduler.h"
#include "types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct spencer_thread    SpencerThread;
typedef struct spencer_scheduler SpencerScheduler;

/* --- SpencerThread function prototypes ------------------------------------ */

SpencerThread     *spencer_thread_new(const byte   *pc,
                                      const char   *sp,
                                      len_t         ncaptures,
                                      const cntr_t *counters,
                                      len_t         ncounters,
                                      const byte   *memory,
                                      len_t         memory_len);
const byte        *spencer_thread_pc(const SpencerThread *self);
void               spencer_thread_set_pc(SpencerThread *self, const byte *pc);
const char        *spencer_thread_sp(const SpencerThread *self);
void               spencer_thread_try_inc_sp(SpencerThread *self);
const char *const *spencer_thread_captures(const SpencerThread *self,
                                           len_t               *ncaptures);
void               spencer_thread_set_capture(SpencerThread *self, len_t idx);
cntr_t             spencer_thread_counter(const SpencerThread *self, len_t idx);
void  spencer_thread_set_counter(SpencerThread *self, len_t idx, cntr_t val);
void  spencer_thread_inc_counter(SpencerThread *self, len_t idx);
void *spencer_thread_memory(const SpencerThread *self, len_t idx);
void  spencer_thread_set_memory(SpencerThread *self,
                                len_t          idx,
                                const void    *val,
                                size_t         size);
SpencerThread *spencer_thread_clone(const SpencerThread *self);
void           spencer_thread_free(SpencerThread *self);

ThreadManager *spencer_thread_manager_new(void);

/* --- SpencerScheduler function prototypes --------------------------------- */

SpencerScheduler *spencer_scheduler_new(const Program *program);
void spencer_scheduler_init(SpencerScheduler *self, const char *text);
void spencer_scheduler_schedule(SpencerScheduler *self, SpencerThread *thread);
void spencer_scheduler_schedule_in_order(SpencerScheduler *self,
                                         SpencerThread    *thread);
int  spencer_scheduler_has_next(const SpencerScheduler *self);
SpencerThread *spencer_scheduler_next(SpencerScheduler *self);
void           spencer_scheduler_reset(SpencerScheduler *self);
void           spencer_scheduler_notify_match(SpencerScheduler *self);
void spencer_scheduler_kill(SpencerScheduler *self, SpencerThread *thread);
SpencerScheduler *spencer_scheduler_clone(const SpencerScheduler *self);
SpencerScheduler *spencer_scheduler_clone_with(const SpencerScheduler *self,
                                               SpencerThread          *thread);
const Program    *spencer_scheduler_program(const SpencerScheduler *self);
void              spencer_scheduler_free(SpencerScheduler *self);

Scheduler *spencer_scheduler_as_scheduler(SpencerScheduler *self);
Scheduler *spencer_scheduler_to_scheduler(SpencerScheduler *self);

#endif /* SPENCER_H */
