#include <stdlib.h>
#include <string.h>

#include "stc/fatp/slice.h"
#include "stc/fatp/vec.h"

#include "lockstep.h"
#include "scheduler.h"

/* --- Type definitions ----------------------------------------------------- */

struct thompson_thread {
    const byte        *pc;
    const char *const *sp;
    cntr_t      *counters; /*<< stc_slice                                     */
    byte        *memory;   /*<< stc_slice                                     */
    const char **captures; /*<< stc_slice                                     */
};

struct thompson_scheduler {
    const Program *prog;
    const char    *text;
    const char    *sp;
    int            in_sync;

    ThompsonThread **curr; /*<< stc_vec                                       */
    ThompsonThread **next; /*<< stc_vec                                       */
    ThompsonThread **sync; /*<< stc_vec                                       */
};

/* --- ThompsonThread function definitions ---------------------------------- */

ThompsonThread *thompson_thread_new(const byte        *pc,
                                    const char *const *sp,
                                    const cntr_t      *counters,
                                    len_t              ncounters,
                                    len_t              memory_len,
                                    len_t              ncaptures)
{
    ThompsonThread *thread = malloc(sizeof(*thread));

    thread->pc = pc;
    thread->sp = sp;

    thread->counters = stc_slice_from_parts(counters, ncounters);
    stc_slice_init(thread->memory, memory_len);
    memset(thread->memory, 0, memory_len * sizeof(*thread->memory));
    stc_slice_init(thread->captures, 2 * ncaptures);
    memset(thread->captures, 0, 2 * ncaptures * sizeof(*thread->captures));

    return thread;
}

const byte *thompson_thread_pc(const ThompsonThread *self) { return self->pc; }

void thompson_thread_set_pc(ThompsonThread *self, const byte *pc)
{
    self->pc = pc;
}

const char *thompson_thread_sp(const ThompsonThread *self) { return *self->sp; }

void thompson_thread_try_inc_sp(ThompsonThread *self) { (void) self; }

int thompson_thread_memoise(ThompsonThread *self,
                            const char     *text,
                            size_t          text_len,
                            len_t           idx)
{
    (void) self;
    (void) text;
    (void) text_len;
    (void) idx;
    return TRUE;
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

const char *const *thompson_thread_captures(const ThompsonThread *self,
                                            len_t                *ncaptures)
{
    if (ncaptures) *ncaptures = stc_slice_len(self->captures) / 2;
    return self->captures;
}

void thompson_thread_set_capture(ThompsonThread *self, len_t idx)
{
    self->captures[idx] = *self->sp;
}

ThompsonThread *thompson_thread_clone(const ThompsonThread *self)
{
    ThompsonThread *t = malloc(sizeof(*t));
    memcpy(t, self, sizeof(*t));

    t->captures = stc_slice_clone(self->captures);
    t->counters = stc_slice_clone(self->counters);
    t->memory   = stc_slice_clone(self->memory);

    return t;
}

void thompson_thread_free(ThompsonThread *self)
{
    stc_slice_free(self->captures);
    stc_slice_free(self->counters);
    stc_slice_free(self->memory);
    free(self);
}

THREAD_MANAGER_DEFINE_WRAPPERS(thompson_thread, ThompsonThread)
THREAD_MANAGER_DEFINE_CONSTRUCTOR_FUNCTION(thompson_thread_manager_new,
                                           thompson_thread)

static int thompson_thread_eq(ThompsonThread *t1, ThompsonThread *t2)
{
    len_t i, len = stc_slice_len(t1->counters);
    int   eq = t1->pc == t2->pc && len == stc_slice_len(t2->counters);

    for (i = 0; i < len && eq; i++) eq = t1->counters[i] == t2->counters[i];

    return eq;
}

static int thompson_threads_contain(ThompsonThread **threads,
                                    ThompsonThread  *thread)
{
    size_t i, len;

    len = stc_vec_len(threads);
    for (i = 0; i < len; i++)
        if (thompson_thread_eq(threads[i], thread)) return TRUE;

    return FALSE;
}

/* --- ThompsonScheduler function definitions ------------------------------- */

ThompsonScheduler *thompson_scheduler_new(const Program *program,
                                          const char    *text)
{
    ThompsonScheduler *s = malloc(sizeof(*s));

    s->prog = program;
    s->text = s->sp = text;
    stc_vec_default_init(s->curr); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(s->next); // NOLINT(bugprone-sizeof-expression)
    stc_vec_default_init(s->sync); // NOLINT(bugprone-sizeof-expression)

    return s;
}

void thompson_scheduler_init(ThompsonScheduler *self, const char *text)
{
    const Program *prog = self->prog;

    (void) text;
    thompson_scheduler_schedule(
        self, thompson_thread_new(prog->insts, &self->sp, prog->counters,
                                  stc_vec_len(prog->counters),
                                  prog->thread_mem_len, prog->ncaptures));
}

void thompson_scheduler_schedule(ThompsonScheduler *self,
                                 ThompsonThread    *thread)
{
    if (!thompson_threads_contain(self->next, thread) &&
        !thompson_threads_contain(self->sync, thread)) {
        switch (*thread->pc) {
            case CHAR:
            case PRED:
                if (stc_vec_is_empty(self->next))
                    // NOLINTNEXTLINE(bugprone-sizeof-expression)
                    stc_vec_push_back(self->sync, thread);
                else
                    // NOLINTNEXTLINE(bugprone-sizeof-expression)
                    stc_vec_push_back(self->next, thread);
                break;

            default:
                // NOLINTNEXTLINE(bugprone-sizeof-expression)
                stc_vec_push_back(self->next, thread);
                break;
        }
    } else {
        thompson_thread_free(thread);
    }
}

void thompson_scheduler_schedule_in_order(ThompsonScheduler *self,
                                          ThompsonThread    *thread)
{
    thompson_scheduler_schedule(self, thread);
}

int thompson_scheduler_has_next(const ThompsonScheduler *self)
{
    return !stc_vec_is_empty(self->curr) || !stc_vec_is_empty(self->next) ||
           !stc_vec_is_empty(self->sync);
}

ThompsonThread *thompson_scheduler_next(ThompsonScheduler *self)
{
    ThompsonThread  *thread = NULL;
    ThompsonThread **tmp;

    if (stc_vec_is_empty(self->curr)) {
        if (self->in_sync) {
            self->in_sync = FALSE;
            self->sp      = stc_utf8_str_next(self->sp);
            thompson_scheduler_init(self, self->text);
        }

        if (stc_vec_is_empty(self->next)) {
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

    if (!stc_vec_is_empty(self->curr)) {
        thread = self->curr[0];
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_remove(self->curr, 0);
        switch (*thread->pc) {
            case CHAR:
            case PRED:
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

    len = stc_vec_len(self->curr);
    for (i = 0; i < len; i++)
        if (self->curr[i]) thompson_thread_free(self->curr[i]);
    stc_vec_clear(self->curr);

    len = stc_vec_len(self->next);
    for (i = 0; i < len; i++)
        if (self->next[i]) thompson_thread_free(self->next[i]);
    stc_vec_clear(self->next);

    len = stc_vec_len(self->sync);
    for (i = 0; i < len; i++)
        if (self->sync[i]) thompson_thread_free(self->sync[i]);
    stc_vec_clear(self->sync);
}

void thompson_scheduler_notify_match(ThompsonScheduler *self)
{
    size_t i, len = stc_vec_len(self->curr);

    for (i = 0; i < len; i++)
        if (self->curr[i]) thompson_thread_free(self->curr[i]);
    stc_vec_clear(self->curr);
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
    stc_vec_free(self->curr);
    stc_vec_free(self->next);
    stc_vec_free(self->sync);
    free(self);
}

SCHEDULER_DEFINE_WRAPPERS(thompson_scheduler, ThompsonScheduler, ThompsonThread)
