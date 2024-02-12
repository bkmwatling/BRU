#ifndef MEMOISATION_H
#define MEMOISATION_H

#include "thread_manager.h"

ThreadManager *memoised_thread_manager_new(ThreadManager *thread_manager);
void           memoised_thread_manager_reset(void *impl);
void           memoised_thread_manager_free(void *impl);

Thread *memoised_thread_manager_spawn_thread(void       *impl,
                                             const byte *pc,
                                             const char *sp);
void    memoised_thread_manager_schedule_thread(void *impl, Thread *t);
void memoised_thread_manager_schedule_thread_in_order(void *_self, Thread *t);
Thread *memoised_thread_manager_next_thread(void *impl);
void    memoised_thread_manager_notify_thread_match(void *impl, Thread *t);
Thread *memoised_thread_manager_clone_thread(void *impl, const Thread *t);
void    memoised_thread_manager_kill_thread(void *impl, Thread *t);
void    memoised_thread_manager_init_memoisation(void  *impl,
                                                 size_t nmemo_insts,
                                                 size_t text_len);

const byte *memoised_thread_pc(void *impl, const Thread *t);
void        memoised_thread_set_pc(void *impl, Thread *t, const byte *pc);
const char *memoised_thread_sp(void *impl, const Thread *t);
void        memoised_thread_inc_sp(void *impl, Thread *t);
int         memoised_thread_memoise(void       *impl,
                                    Thread     *t,
                                    const char *text,
                                    size_t      text_len,
                                    len_t       idx);
cntr_t      memoised_thread_counter(void *impl, const Thread *t, len_t idx);
void  memoised_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val);
void  memoised_thread_inc_counter(void *impl, Thread *t, len_t idx);
void *memoised_thread_memory(void *impl, const Thread *t, len_t idx);
void  memoised_thread_set_memory(void       *impl,
                                 Thread     *t,
                                 len_t       idx,
                                 const void *val,
                                 size_t      size);
const char *const      *
memoised_thread_captures(void *impl, const Thread *t, len_t *ncaptures);
void memoised_thread_set_capture(void *impl, Thread *t, len_t idx);

#endif
