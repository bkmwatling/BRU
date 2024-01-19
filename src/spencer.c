#include <stdlib.h>

#include "stc/fatp/vec.h"

#include "scheduler.h"
#include "spencer.h"

/* --- Type definitions ----------------------------------------------------- */

struct spencer_thread {
    const byte  *pc;
    const char  *sp;
    const char **captures;
    len_t        ncaptures;
    cntr_t      *counters;
    len_t        ncounters;
    byte        *memory;
    len_t        memory_len;
};

struct spencer_scheduler {
    const Program  *prog;
    size_t          in_order_idx;
    SpencerThread  *active;
    SpencerThread **stack;
};

/* --- SpencerThread function definitions ----------------------------------- */

SpencerThread *spencer_thread_new(const byte   *pc,
                                  const char   *sp,
                                  len_t         ncaptures,
                                  const cntr_t *counters,
                                  len_t         ncounters,
                                  const byte   *memory,
                                  len_t         memory_len)
{
    SpencerThread *thread = malloc(sizeof(SpencerThread));

    thread->pc = pc;
    thread->sp = sp;

    thread->captures = malloc(2 * ncaptures * sizeof(char *));
    memset(thread->captures, 0, 2 * ncaptures * sizeof(char *));
    thread->ncaptures = ncaptures;

    thread->counters = malloc(ncounters * sizeof(cntr_t));
    memcpy(thread->counters, counters, ncounters * sizeof(cntr_t));
    thread->ncounters = ncounters;

    thread->memory = malloc(memory_len * sizeof(byte));
    memcpy(thread->memory, memory, memory_len * sizeof(byte));
    thread->memory_len = memory_len;

    return thread;
}

const byte *spencer_thread_pc(const SpencerThread *self) { return self->pc; }

void spencer_thread_set_pc(SpencerThread *self, const byte *pc)
{
    self->pc = pc;
}

const char *spencer_thread_sp(const SpencerThread *self) { return self->sp; }

void spencer_thread_try_inc_sp(SpencerThread *self) { self->sp++; }

const char *const *spencer_thread_captures(const SpencerThread *self,
                                           len_t               *ncaptures)
{
    *ncaptures = self->ncaptures;
    return self->captures;
}

void spencer_thread_set_capture(SpencerThread *self, len_t idx)
{
    self->captures[idx] = self->sp;
}

cntr_t spencer_thread_counter(const SpencerThread *self, len_t idx)
{
    return self->counters[idx];
}

void spencer_thread_set_counter(SpencerThread *self, len_t idx, cntr_t val)
{
    self->counters[idx] = val;
}

void spencer_thread_inc_counter(SpencerThread *self, len_t idx)
{
    self->counters[idx]++;
}

void *spencer_thread_memory(const SpencerThread *self, len_t idx)
{
    return self->memory + idx;
}

void spencer_thread_set_memory(SpencerThread *self,
                               len_t          idx,
                               const void    *val,
                               size_t         size)
{
    memcpy(self->memory + idx, val, size);
}

SpencerThread *spencer_thread_clone(const SpencerThread *self)
{
    SpencerThread *t = malloc(sizeof(SpencerThread));
    memcpy(t, self, sizeof(SpencerThread));

    t->captures = malloc(2 * t->ncaptures * sizeof(char **));
    memcpy(t->captures, self->captures, 2 * t->ncaptures * sizeof(char **));
    t->counters = malloc(t->ncounters * sizeof(cntr_t));
    memcpy(t->counters, self->counters, t->ncounters * sizeof(cntr_t));
    t->memory = malloc(t->memory_len * sizeof(byte));
    memcpy(t->memory, self->memory, t->memory_len * sizeof(byte));

    return t;
}

void spencer_thread_free(SpencerThread *self)
{
    free(self->captures);
    free(self->counters);
    free(self->memory);
    free(self);
}

THREAD_MANAGER_DEFINE_WRAPPERS(spencer_thread, SpencerThread)
THREAD_MANAGER_DEFINE_CONSTRUCTOR_FUNCTION(spencer_thread_manager_new,
                                           spencer_thread)

/* --- SpencerScheduler function definitions -------------------------------- */

SpencerScheduler *spencer_scheduler_new(const Program *program)
{
    SpencerScheduler *s = malloc(sizeof(SpencerScheduler));

    s->prog         = program;
    s->in_order_idx = 0;
    s->active       = NULL;
    s->stack        = NULL;
    stc_vec_default_init(s->stack);

    return s;
}

void spencer_scheduler_init(SpencerScheduler *self, const char *text)
{
    unsigned long  text_len = strlen(text);
    const Program *prog     = self->prog;

    spencer_scheduler_schedule(
        self,
        spencer_thread_new(prog->insts, text, prog->ncaptures, prog->counters,
                           prog->ncounters, prog->memory, prog->mem_len));

    for (; text_len > 0; text_len--) {
        spencer_scheduler_schedule(
            self,
            spencer_thread_new(prog->insts, text + text_len, prog->ncaptures,
                               prog->counters, prog->ncounters, prog->memory,
                               prog->mem_len));
    }
}

void spencer_scheduler_schedule(SpencerScheduler *self, SpencerThread *thread)
{
    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    if (self->active) {
        stc_vec_push(self->stack, thread);
    } else {
        self->active = thread;
    }
}

void spencer_scheduler_schedule_in_order(SpencerScheduler *self,
                                         SpencerThread    *thread)
{
    size_t len = stc_vec_len_unsafe(self->stack);

    if (self->in_order_idx > len) {
        spencer_scheduler_schedule(self, thread);
        self->in_order_idx = len;
    } else if (self->in_order_idx == len) {
        stc_vec_push(self->stack, thread);
    } else {
        stc_vec_insert(self->stack, self->in_order_idx, thread);
    }
}

int spencer_scheduler_has_next(const SpencerScheduler *self)
{
    return self->active != NULL || !stc_vec_is_empty(self->stack);
}

SpencerThread *spencer_scheduler_next(SpencerScheduler *self)
{
    SpencerThread *thread = self->active;

    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    self->active       = NULL;
    if (thread == NULL && !stc_vec_is_empty(self->stack))
        thread = stc_vec_pop(self->stack);

    return thread;
}

void spencer_scheduler_reset(SpencerScheduler *self)
{
    size_t i, len = stc_vec_len(self->stack);

    self->in_order_idx = 0;
    if (self->active) {
        spencer_thread_free(self->active);
        self->active = NULL;
    }

    for (i = 0; i < len; i++) {
        if (self->stack[i]) spencer_thread_free(self->stack[i]);
    }
    stc_vec_clear(self->stack);
}

void spencer_scheduler_notify_match(SpencerScheduler *self)
{
    spencer_scheduler_reset(self);
}

void spencer_scheduler_kill(SpencerScheduler *self, SpencerThread *thread)
{
    (void) self;
    spencer_thread_free(thread);
}

SpencerScheduler *spencer_scheduler_clone(const SpencerScheduler *self)
{
    return spencer_scheduler_new(self->prog);
}

SpencerScheduler *spencer_scheduler_clone_with(const SpencerScheduler *self,
                                               SpencerThread          *thread)
{
    SpencerScheduler *s = spencer_scheduler_clone(self);
    spencer_scheduler_schedule(s, thread);
    return s;
}

const Program *spencer_scheduler_program(const SpencerScheduler *self)
{
    return self->prog;
}

void spencer_scheduler_free(SpencerScheduler *self)
{
    spencer_scheduler_reset(self);
    stc_vec_free(self->stack);
    free(self);
}

SCHEDULER_DEFINE_WRAPPERS(spencer_scheduler, SpencerScheduler, SpencerThread)
