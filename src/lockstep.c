#include <assert.h>
#include <stdlib.h>

#define STC_VEC_ENABLE_SHORT_NAMES
#include "stc/fatp/vec.h"

#include "lockstep.h"
#include "scheduler.h"

/* --- Type definitions ----------------------------------------------------- */

struct thompson_thread {
    const byte        *pc;
    const char *const *sp;
    const char       **captures;
    len_t              ncaptures;
    cntr_t            *counters;
    len_t              ncounters;
    byte              *memory;
    len_t              memory_len;
};

struct thompson_scheduler {
    const Program *prog;
    const char    *text;
    const char    *sp;
    int            in_sync;

    ThompsonThread **curr;
    ThompsonThread **next;
    ThompsonThread **sync;
};

/* --- ThompsonThread function definitions ---------------------------------- */

ThompsonThread *thompson_thread_new(const byte        *pc,
                                    const char *const *sp,
                                    len_t              ncaptures,
                                    const cntr_t      *counters,
                                    len_t              ncounters,
                                    const byte        *memory,
                                    len_t              memory_len)
{
    ThompsonThread *thread = malloc(sizeof(ThompsonThread));

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

const byte *thompson_thread_pc(const ThompsonThread *self) { return self->pc; }

void thompson_thread_set_pc(ThompsonThread *self, const byte *pc)
{
    self->pc = pc;
}

const char *thompson_thread_sp(const ThompsonThread *self) { return *self->sp; }

void thompson_thread_try_inc_sp(ThompsonThread *self) { (void) self; }

const char *const *thompson_thread_captures(const ThompsonThread *self,
                                            len_t                *ncaptures)
{
    *ncaptures = self->ncaptures;
    return self->captures;
}

void thompson_thread_set_capture(ThompsonThread *self, len_t idx)
{
    self->captures[idx] = *self->sp;
}

cntr_t thompson_thread_counter(const ThompsonThread *self, len_t idx)
{
    return self->counters[idx];
}

void thompson_thread_set_counter(ThompsonThread *self, len_t idx, cntr_t val)
{
    self->counters[idx] = val;
}

void thompson_thread_inc_counter(ThompsonThread *self, len_t idx)
{
    self->counters[idx]++;
}

void *thompson_thread_memory(const ThompsonThread *self, len_t idx)
{
    return self->memory + idx;
}

void thompson_thread_set_memory(ThompsonThread *self,
                                len_t           idx,
                                const void     *val,
                                size_t          size)
{
    memcpy(self->memory + idx, val, size);
}

ThompsonThread *thompson_thread_clone(const ThompsonThread *self)
{
    ThompsonThread *t = malloc(sizeof(ThompsonThread));
    memcpy(t, self, sizeof(ThompsonThread));

    t->captures = malloc(2 * t->ncaptures * sizeof(char **));
    memcpy(t->captures, self->captures, 2 * t->ncaptures * sizeof(char **));
    t->counters = malloc(t->ncounters * sizeof(cntr_t));
    memcpy(t->counters, self->counters, t->ncounters * sizeof(cntr_t));
    t->memory = malloc(t->memory_len * sizeof(byte));
    memcpy(t->memory, self->memory, t->memory_len * sizeof(byte));

    return t;
}

void thompson_thread_free(ThompsonThread *self)
{
    free(self->captures);
    free(self->counters);
    free(self->memory);
    free(self);
}

THREAD_MANAGER_DEFINE_WRAPPERS(thompson_thread, ThompsonThread)
THREAD_MANAGER_DEFINE_CONSTRUCTOR_FUNCTION(thompson_thread_manager_new,
                                           thompson_thread)

static int thompson_thread_eq(ThompsonThread *t1, ThompsonThread *t2)
{
    len_t i, len;
    int   eq = t1->pc == t2->pc && t1->ncounters == t2->ncounters;

    len = t1->ncounters;
    for (i = 0; i < len && eq; i++) eq = t1->counters[i] == t2->counters[i];

    return eq;
}

static int thompson_threads_contains(ThompsonThread **threads,
                                     ThompsonThread  *thread)
{
    size_t i, len;

    len = vec_len(threads);
    for (i = 0; i < len; i++) {
        if (thompson_thread_eq(threads[i], thread)) return TRUE;
    }

    return FALSE;
}

/* --- ThompsonScheduler function definitions ------------------------------- */

ThompsonScheduler *thompson_scheduler_new(const Program *program,
                                          const char    *text)
{
    ThompsonScheduler *s = malloc(sizeof(ThompsonScheduler));

    s->prog = program;
    s->text = s->sp = text;
    s->curr = s->next = s->sync = NULL;
    vec_default_init(s->curr);
    vec_default_init(s->next);
    vec_default_init(s->sync);

    return s;
}

void thompson_scheduler_init(ThompsonScheduler *self, const char *text)
{
    const Program *prog = self->prog;

    thompson_scheduler_schedule(
        self, thompson_thread_new(prog->insts, &self->sp, prog->ncaptures,
                                  prog->counters, prog->ncounters, prog->memory,
                                  prog->mem_len));
    (void) text;
}

void thompson_scheduler_schedule(ThompsonScheduler *self,
                                 ThompsonThread    *thread)
{
    if (!thompson_threads_contains(self->next, thread) &&
        !thompson_threads_contains(self->sync, thread)) {
        switch (*thread->pc) {
            case CHAR:
            case PRED:
            case LSWITCH:
                if (vec_is_empty(self->next)) {
                    vec_push(self->sync, thread);
                } else {
                    vec_push(self->next, thread);
                }
                break;

            default: vec_push(self->next, thread); break;
        }
    } else {
        thompson_thread_free(thread);
    }
}

int thompson_scheduler_has_next(const ThompsonScheduler *self)
{
    return !vec_is_empty(self->curr) || !vec_is_empty(self->next) ||
           !vec_is_empty(self->sync);
}

ThompsonThread *thompson_scheduler_next(ThompsonScheduler *self)
{
    ThompsonThread  *thread = NULL;
    ThompsonThread **tmp;

    if (vec_is_empty(self->curr)) {
        if (self->in_sync) {
            self->in_sync = FALSE;
            self->sp      = utf8_str_next(self->sp);
            thompson_scheduler_init(self, self->text);
        }

        if (vec_is_empty(self->next)) {
            self->in_sync = TRUE;
            tmp           = self->curr;
            self->curr    = self->sync;
            self->sync    = tmp;
        } else {
            tmp        = self->curr;
            self->curr = self->next;
            self->next = tmp;
        }
    }

    if (!vec_is_empty(self->curr)) {
        thread = self->curr[0];
        vec_remove(self->curr, 0);
        switch (*thread->pc) {
            case CHAR:
            case PRED:
            case LSWITCH:
                if (!self->in_sync) {
                    thompson_scheduler_schedule(self, thread);
                    thread = thompson_scheduler_next(self);
                }
                break;

            default: break;
        }
    }

    return thread;
}

void thompson_scheduler_reset(ThompsonScheduler *self)
{
    size_t i, len;

    self->sp      = self->text;
    self->in_sync = FALSE;

    len = vec_len(self->curr);
    for (i = 0; i < len; i++) {
        if (self->curr[i]) thompson_thread_free(self->curr[i]);
    }
    vec_clear(self->curr);

    len = vec_len(self->next);
    for (i = 0; i < len; i++) {
        if (self->next[i]) thompson_thread_free(self->next[i]);
    }
    vec_clear(self->next);

    len = vec_len(self->sync);
    for (i = 0; i < len; i++) {
        if (self->sync[i]) thompson_thread_free(self->sync[i]);
    }
    vec_clear(self->sync);
}

void thompson_scheduler_notify_match(ThompsonScheduler *self)
{
    size_t i, len = vec_len(self->curr);

    for (i = 0; i < len; i++) {
        if (self->curr[i]) thompson_thread_free(self->curr[i]);
    }
    vec_clear(self->curr);
}

void thompson_scheduler_kill(ThompsonScheduler *self, ThompsonThread *thread)
{
    (void) self;
    thompson_thread_free(thread);
}

ThompsonScheduler *thompson_scheduler_clone(const ThompsonScheduler *self)
{
    return thompson_scheduler_new(self->prog, self->text);
}

ThompsonScheduler *thompson_scheduler_clone_with(const ThompsonScheduler *self,
                                                 ThompsonThread *thread)
{
    ThompsonScheduler *s = thompson_scheduler_clone(self);
    thread->sp           = &s->sp;
    thompson_scheduler_schedule(s, thread);
    return s;
}

const Program *thompson_scheduler_program(const ThompsonScheduler *self)
{
    return self->prog;
}

void thompson_scheduler_free(ThompsonScheduler *self)
{
    thompson_scheduler_reset(self);
    vec_free(self->curr);
    vec_free(self->next);
    vec_free(self->sync);
    free(self);
}

SCHEDULER_DEFINE_WRAPPERS(thompson_scheduler, ThompsonScheduler, ThompsonThread)
