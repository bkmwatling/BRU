#include <stdlib.h>
#include <string.h>

#include "stc/fatp/slice.h"
#include "stc/fatp/vec.h"

#include "scheduler.h"
#include "spencer.h"

/* --- Type definitions ----------------------------------------------------- */

struct spencer_thread {
    const byte  *pc;
    const char  *sp;
    byte        *memoisation_memory;
    cntr_t      *counters; /*<< stc_slice                                     */
    byte        *memory;   /*<< stc_slice                                     */
    const char **captures; /*<< stc_slice                                     */
};

struct spencer_scheduler {
    const Program  *prog;
    byte           *memoisation_memory;
    size_t          in_order_idx;
    SpencerThread  *active;
    SpencerThread **stack; /*<< stc_vec                                       */
};

/* --- SpencerThread function definitions ----------------------------------- */

SpencerThread *spencer_thread_new(const byte   *pc,
                                  const char   *sp,
                                  byte         *memoisation_memory,
                                  const cntr_t *counters,
                                  len_t         ncounters,
                                  len_t         memory_len,
                                  len_t         ncaptures)
{
    SpencerThread *thread = malloc(sizeof(*thread));

    thread->pc                 = pc;
    thread->sp                 = sp;
    thread->memoisation_memory = memoisation_memory;

    thread->counters = stc_slice_from_parts(counters, ncounters);
    stc_slice_init(thread->memory, memory_len);
    memset(thread->memory, 0, memory_len * sizeof(*thread->memory));
    stc_slice_init(thread->captures, 2 * ncaptures);
    memset(thread->captures, 0, 2 * ncaptures * sizeof(*thread->captures));

    return thread;
}

const byte *spencer_thread_pc(const SpencerThread *self) { return self->pc; }

void spencer_thread_set_pc(SpencerThread *self, const byte *pc)
{
    self->pc = pc;
}

const char *spencer_thread_sp(const SpencerThread *self) { return self->sp; }

void spencer_thread_try_inc_sp(SpencerThread *self) { self->sp++; }

int spencer_thread_memoise(SpencerThread *self,
                           const char    *text,
                           size_t         text_len,
                           len_t          idx)
{
    size_t i = idx * text_len + self->sp - text;
    if (self->memoisation_memory[i]) return FALSE;

    self->memoisation_memory[i] = TRUE;
    return TRUE;
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

const char *const *spencer_thread_captures(const SpencerThread *self,
                                           len_t               *ncaptures)
{
    if (ncaptures) *ncaptures = stc_slice_len(self->captures) / 2;
    return self->captures;
}

void spencer_thread_set_capture(SpencerThread *self, len_t idx)
{
    self->captures[idx] = self->sp;
}

SpencerThread *spencer_thread_clone(const SpencerThread *self)
{
    SpencerThread *t = malloc(sizeof(*t));
    memcpy(t, self, sizeof(*t));

    t->captures = stc_slice_clone(self->captures);
    t->counters = stc_slice_clone(self->counters);
    t->memory   = stc_slice_clone(self->memory);

    return t;
}

void spencer_thread_free(SpencerThread *self)
{
    stc_slice_free(self->captures);
    stc_slice_free(self->counters);
    stc_slice_free(self->memory);
    free(self);
}

THREAD_MANAGER_DEFINE_WRAPPERS(spencer_thread, SpencerThread)
THREAD_MANAGER_DEFINE_CONSTRUCTOR_FUNCTION(spencer_thread_manager_new,
                                           spencer_thread)

/* --- SpencerScheduler function definitions -------------------------------- */

SpencerScheduler *spencer_scheduler_new(const Program *program)
{
    SpencerScheduler *s = malloc(sizeof(*s));

    s->prog               = program;
    s->memoisation_memory = NULL;
    s->in_order_idx       = 0;
    s->active             = NULL;
    stc_vec_default_init(s->stack); // NOLINT(bugprone-sizeof-expression)

    return s;
}

void spencer_scheduler_init(SpencerScheduler *self, const char *text)
{
    const Program *prog     = self->prog;
    size_t         text_len = strlen(text);

    stc_slice_free(self->memoisation_memory);
    stc_slice_init(self->memoisation_memory,
                   self->prog->nmemo_insts * text_len * sizeof(byte));
    memset(self->memoisation_memory, 0,
           self->prog->nmemo_insts * text_len * sizeof(byte));

    spencer_scheduler_schedule(
        self, spencer_thread_new(prog->insts, text, self->memoisation_memory,
                                 prog->counters, stc_vec_len(prog->counters),
                                 prog->thread_mem_len, prog->ncaptures));

    while (text_len > 0)
        spencer_scheduler_schedule(
            self, spencer_thread_new(prog->insts, text + text_len--,
                                     self->memoisation_memory, prog->counters,
                                     stc_vec_len(prog->counters),
                                     prog->thread_mem_len, prog->ncaptures));
}

void spencer_scheduler_schedule(SpencerScheduler *self, SpencerThread *thread)
{
    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    if (self->active)
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    else
        self->active = thread;
}

void spencer_scheduler_schedule_in_order(SpencerScheduler *self,
                                         SpencerThread    *thread)
{
    size_t len = stc_vec_len_unsafe(self->stack);

    if (self->in_order_idx > len) {
        spencer_scheduler_schedule(self, thread);
        self->in_order_idx = len;
    } else if (self->in_order_idx == len) {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    } else {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_insert(self->stack, self->in_order_idx, thread);
    }
}

int spencer_scheduler_has_next(const SpencerScheduler *self)
{
    return self->active || !stc_vec_is_empty(self->stack);
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

    if (self->memoisation_memory)
        memset(self->memoisation_memory, 0,
               stc_slice_len(self->memoisation_memory));
    self->in_order_idx = 0;
    if (self->active) {
        spencer_thread_free(self->active);
        self->active = NULL;
    }

    for (i = 0; i < len; i++)
        if (self->stack[i]) spencer_thread_free(self->stack[i]);
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
    SpencerScheduler *s   = spencer_scheduler_clone(self);
    s->memoisation_memory = stc_slice_clone(self->memoisation_memory);
    spencer_scheduler_schedule(s, thread);
    return s;
}

const Program *spencer_scheduler_program(const SpencerScheduler *self)
{
    return self->prog;
}

void spencer_scheduler_free(SpencerScheduler *self)
{
    stc_slice_free(self->memoisation_memory);
    self->memoisation_memory = NULL;
    spencer_scheduler_reset(self);
    stc_vec_free(self->stack);
    free(self);
}

SCHEDULER_DEFINE_WRAPPERS(spencer_scheduler, SpencerScheduler, SpencerThread)
