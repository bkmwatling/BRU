#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STC_SV_ENABLE_SHORT_NAMES
#include "stc/fatp/string_view.h"

#include "program.h"
#include "srvm.h"

/* --- Type definitions ----------------------------------------------------- */

struct srvm {
    ThreadManager *thread_manager;
    Scheduler     *scheduler;
    len_t          ncaptures;
    const char   **captures;
};

/* --- Private function prototypes ------------------------------------------ */

static int srvm_run(const char    *text,
                    ThreadManager *thread_manager,
                    Scheduler     *scheduler,
                    const char   **captures);

/* --- Public function definitions ------------------------------------------ */

SRVM *srvm_new(ThreadManager *thread_manager, Scheduler *scheduler)
{
    SRVM *srvm = malloc(sizeof(SRVM));

    srvm->thread_manager = thread_manager;
    srvm->scheduler      = scheduler;
    srvm->ncaptures      = scheduler_program(scheduler)->ncaptures;
    srvm->captures       = malloc(2 * srvm->ncaptures * sizeof(char *));
    memset(srvm->captures, 0, 2 * srvm->ncaptures * sizeof(char *));

    return srvm;
}

void srvm_free(SRVM *self)
{
    free(self->thread_manager);
    scheduler_free(self->scheduler);
    free(self->captures);
    free(self);
}

int srvm_match(SRVM *self, const char *text)
{
    if (text == NULL) return 0;

    memset(self->captures, 0, 2 * self->ncaptures * sizeof(char *));
    scheduler_reset(self->scheduler);
    scheduler_init(self->scheduler, text);
    return srvm_run(text, self->thread_manager, self->scheduler,
                    self->captures);
}

StringView srvm_capture(SRVM *self, len_t idx)
{
    if (idx >= self->ncaptures) return (StringView){ 0, NULL };

    return sv_from_range(self->captures[2 * idx], self->captures[2 * idx + 1]);
}

StringView *srvm_captures(SRVM *self, len_t *ncaptures)
{
    len_t       i;
    StringView *captures = malloc(self->ncaptures * sizeof(StringView));

    if (ncaptures) *ncaptures = self->ncaptures;
    for (i = 0; i < self->ncaptures; i++)
        captures[i] =
            sv_from_range(self->captures[2 * i], self->captures[2 * i + 1]);

    return captures;
}

int srvm_matches(ThreadManager *thread_manager,
                 Scheduler     *scheduler,
                 const char    *text)
{
    SRVM *srvm    = srvm_new(thread_manager, scheduler);
    int   matches = srvm_match(srvm, text);
    srvm_free(srvm);
    return matches;
}

/* --- Private function definitions ----------------------------------------- */

static int srvm_run(const char    *text,
                    ThreadManager *thread_manager,
                    Scheduler     *scheduler,
                    const char   **captures)
{
    int            matched = 0, cond;
    const Program *prog    = scheduler_program(scheduler);
    void          *thread, *t;
    const byte    *pc;
    const char    *sp, *codepoint;
    len_t          ncaptures, intervals_len, k;
    offset_t       x, y;
    cntr_t         cval, n;
    Interval      *intervals;
    Scheduler     *s;

    while ((thread = scheduler_next(scheduler))) {
        if ((sp = thread_manager->sp(thread)) != text && sp[-1] == '\0')
            continue;

        pc = thread_manager->pc(thread);
        switch (*pc++) {
            case NOOP: /* fallthrough */
            case START:
                thread_manager->set_pc(thread, pc);
                scheduler_schedule(scheduler, thread);

            /* TODO: check */
            case MSTART:
                matched = 1;
                thread_manager->set_pc(thread, pc);
                scheduler_schedule(scheduler, thread);
                break;

            case MATCH:
                scheduler_notify_match(scheduler);
                matched = 1;
                if (captures)
                    memcpy(captures,
                           thread_manager->captures(thread, &ncaptures),
                           2 * ncaptures * sizeof(char *));
                thread_manager->free(thread);
                break;

            case BEGIN:
                if (sp == text) {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case END:
                if (*sp == '\0') {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case CHAR: /* fallthrough */
            case NCHAR:
                MEMPOP(codepoint, pc, const char *);
                if (*sp && utf8_cmp(codepoint, sp) == 0) {
                    thread_manager->set_pc(thread, pc);
                    thread_manager->try_inc_sp(thread);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            /* TODO: */
            case MCHAR: break;

            case PRED: /* fallthrough */
            case NPRED:
                MEMPOP(intervals_len, pc, len_t);
                MEMPOP(k, pc, len_t);
                intervals = (Interval *) (prog->aux + k);
                if (*sp && intervals_predicate(intervals, intervals_len, sp)) {
                    thread_manager->set_pc(thread, pc);
                    thread_manager->try_inc_sp(thread);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            /* TODO: */
            case MPRED: break;

            case SAVE:
                MEMPOP(k, pc, len_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->set_capture(thread, k);
                scheduler_schedule(scheduler, thread);
                break;

            case JMP:
                MEMPOP(x, pc, offset_t);
                thread_manager->set_pc(thread, pc + x);
                scheduler_schedule(scheduler, thread);
                break;

            case SPLIT:
                t = thread_manager->clone(thread);
                MEMPOP(x, pc, offset_t);
                thread_manager->set_pc(thread, pc + x);
                MEMPOP(y, pc, offset_t);
                thread_manager->set_pc(t, pc + y);
                scheduler_schedule(scheduler, thread);
                scheduler_schedule(scheduler, t);
                break;

            /* TODO: */
            case GSPLIT: break;
            case LSPLIT: break;

            case TSWITCH:
                MEMPOP(k, pc, len_t);
                for (; k > 0; k--) {
                    MEMPOP(x, pc, offset_t);
                    t = thread_manager->clone(thread);
                    thread_manager->set_pc(t, pc + x);
                    scheduler_schedule(scheduler, t);
                }
                thread_manager->free(thread);
                break;

            /* TODO: */
            case LSWITCH: break;

            case EPSSET:
                MEMPOP(k, pc, len_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->set_memory(thread, k, &sp, sizeof(sp));
                scheduler_schedule(scheduler, thread);
                break;

            case EPSCHK:
                MEMPOP(k, pc, len_t);
                if (*(char **) thread_manager->memory(thread, k) < sp) {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case RESET:
                MEMPOP(k, pc, len_t);
                MEMPOP(cval, pc, cntr_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->set_counter(thread, k, cval);
                scheduler_schedule(scheduler, thread);
                break;

            case CMP:
                MEMPOP(k, pc, len_t);
                MEMPOP(n, pc, cntr_t);
                cval = thread_manager->counter(thread, k);
                switch (*pc++) {
                    case LT: cond = cval < n;
                    case LE: cond = cval <= n;
                    case EQ: cond = cval == n;
                    case NE: cond = cval != n;
                    case GE: cond = cval >= n;
                    case GT: cond = cval > n;
                }

                if (cond) {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case INC:
                MEMPOP(k, pc, len_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->inc_counter(thread, k);
                scheduler_schedule(scheduler, thread);
                break;

            case ZWA:
                t = thread_manager->clone(thread);
                MEMPOP(x, pc, offset_t);
                thread_manager->set_pc(t, pc + x);
                MEMPOP(y, pc, offset_t);
                thread_manager->set_pc(thread, pc + y);
                s = malloc(sizeof(Scheduler));
                scheduler_copy_with(s, scheduler, t);

                if (srvm_run(text, thread_manager, s, NULL) == *pc) {
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                scheduler_free(s);
                break;
        }
    }

    return matched;
}
