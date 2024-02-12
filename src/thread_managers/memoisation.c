#include "memoisation.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    byte  *memoisation_memory;
    size_t nmemo_insts;
    size_t text_len;

    ThreadManager *__manager;
} MemoisedThreadManager;

/* --- API Routine ---------------------------------------------------------- */

ThreadManager *memoised_thread_manager_new(ThreadManager *thread_manager)
{
    MemoisedThreadManager *mtm = malloc(sizeof(*mtm));
    ThreadManager         *tm  = malloc(sizeof(*tm));

    mtm->memoisation_memory = NULL;
    mtm->text_len           = 0;
    mtm->nmemo_insts        = 0;
    mtm->__manager          = thread_manager;

    THREAD_MANAGER_SET_ALL_FUNCS(tm, memoised);
    tm->impl = mtm;

    return tm;
}

void memoised_thread_manager_reset(void *_self)
{
    MemoisedThreadManager *self = (MemoisedThreadManager *) _self;
    thread_manager_reset(self->__manager);
    if (self->memoisation_memory)
        memset(self->memoisation_memory, 0,
               self->text_len * self->nmemo_insts *
                   sizeof(*self->memoisation_memory));
}

void memoised_thread_manager_free(void *_self)
{
    free(((MemoisedThreadManager *) _self)->memoisation_memory);
    thread_manager_free(((MemoisedThreadManager *) _self)->__manager);
    free(_self);
}

Thread *memoised_thread_manager_spawn_thread(void       *_self,
                                             const byte *pc,
                                             const char *sp)
{
    return thread_manager_spawn_thread(
        ((MemoisedThreadManager *) _self)->__manager, pc, sp);
}

void memoised_thread_manager_schedule_thread(void *_self, Thread *t)
{
    thread_manager_schedule_thread(((MemoisedThreadManager *) _self)->__manager,
                                   t);
}

void memoised_thread_manager_schedule_thread_in_order(void *_self, Thread *t)
{
    thread_manager_schedule_thread_in_order(
        ((MemoisedThreadManager *) _self)->__manager, t);
}

Thread *memoised_thread_manager_next_thread(void *_self)
{
    return thread_manager_next_thread(
        ((MemoisedThreadManager *) _self)->__manager);
}

void memoised_thread_manager_notify_thread_match(void *_self, Thread *t)
{
    thread_manager_notify_thread_match(
        ((MemoisedThreadManager *) _self)->__manager, t);
}

Thread *memoised_thread_manager_clone_thread(void *_self, const Thread *t)
{
    return thread_manager_clone_thread(
        ((MemoisedThreadManager *) _self)->__manager, t);
}

void memoised_thread_manager_kill_thread(void *_self, Thread *t)
{
    thread_manager_kill_thread(((MemoisedThreadManager *) _self)->__manager, t);
}

const byte *memoised_thread_pc(void *_self, const Thread *t)
{
    return thread_manager_pc(((MemoisedThreadManager *) _self)->__manager, t);
}

void memoised_thread_set_pc(void *_self, Thread *t, const byte *pc)
{
    thread_manager_set_pc(((MemoisedThreadManager *) _self)->__manager, t, pc);
}

const char *memoised_thread_sp(void *_self, const Thread *t)
{
    return thread_manager_sp(((MemoisedThreadManager *) _self)->__manager, t);
}

void memoised_thread_inc_sp(void *_self, Thread *t)
{
    thread_manager_inc_sp(((MemoisedThreadManager *) _self)->__manager, t);
}

void memoised_thread_manager_init_memoisation(void  *_self,
                                              size_t nmemo_insts,
                                              size_t text_len)
{
    MemoisedThreadManager *self = (MemoisedThreadManager *) _self;

    if (self->memoisation_memory) {
        if (self->nmemo_insts == nmemo_insts && self->text_len == text_len) {
            memset(self->memoisation_memory, FALSE,
                   self->nmemo_insts * self->text_len *
                       sizeof(*self->memoisation_memory));
            return;
        } else {
            free(self->memoisation_memory);
        }
    }

    self->nmemo_insts = nmemo_insts;
    self->text_len    = text_len;
    self->memoisation_memory =
        malloc(nmemo_insts * text_len * sizeof(*self->memoisation_memory));
    memset(self->memoisation_memory, FALSE,
           nmemo_insts * text_len * sizeof(*self->memoisation_memory));
}

int memoised_thread_memoise(void       *_self,
                            Thread     *t,
                            const char *text,
                            size_t      text_len,
                            len_t       idx)
{
    MemoisedThreadManager *self = (MemoisedThreadManager *) _self;
    size_t                 i    = idx * text_len + t->sp - text;
    if (self->memoisation_memory[i]) return FALSE;

    self->memoisation_memory[i] = TRUE;
    return TRUE;
}

cntr_t memoised_thread_counter(void *_self, const Thread *t, len_t idx)
{
    return thread_manager_counter(((MemoisedThreadManager *) _self)->__manager,
                                  t, idx);
}

void memoised_thread_set_counter(void *_self, Thread *t, len_t idx, cntr_t val)
{
    thread_manager_set_counter(((MemoisedThreadManager *) _self)->__manager, t,
                               idx, val);
}

void memoised_thread_inc_counter(void *_self, Thread *t, len_t idx)
{
    thread_manager_inc_counter(((MemoisedThreadManager *) _self)->__manager, t,
                               idx);
}

void *memoised_thread_memory(void *_self, const Thread *t, len_t idx)
{
    return thread_manager_memory(((MemoisedThreadManager *) _self)->__manager,
                                 t, idx);
}

void memoised_thread_set_memory(void       *_self,
                                Thread     *t,
                                len_t       idx,
                                const void *val,
                                size_t      size)
{
    thread_manager_set_memory(((MemoisedThreadManager *) _self)->__manager, t,
                              idx, val, size);
}

const char *const *
memoised_thread_captures(void *_self, const Thread *t, len_t *ncaptures)
{
    return thread_manager_captures(((MemoisedThreadManager *) _self)->__manager,
                                   t, ncaptures);
}

void memoised_thread_set_capture(void *_self, Thread *t, len_t idx)
{
    thread_manager_set_capture(((MemoisedThreadManager *) _self)->__manager, t,
                               idx);
}

/* --- */
