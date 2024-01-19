#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srvm.h"
#include "stc/fatp/vec.h"

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

StcStringView srvm_capture(SRVM *self, len_t idx)
{
    if (idx >= self->ncaptures) return (StcStringView){ 0, NULL };

    return stc_sv_from_range(self->captures[2 * idx],
                             self->captures[2 * idx + 1]);
}

StcStringView *srvm_captures(SRVM *self, len_t *ncaptures)
{
    len_t          i;
    StcStringView *captures = malloc(self->ncaptures * sizeof(StcStringView));

    if (ncaptures) *ncaptures = self->ncaptures;
    for (i = 0; i < self->ncaptures; i++)
        captures[i] =
            stc_sv_from_range(self->captures[2 * i], self->captures[2 * i + 1]);

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

// XXX: does not check for failed mallocs
static byte **init_memoised(const Program *prog, const char *text)
{
    len_t  i;
    size_t len = strlen(text) + 1;
    byte **m   = malloc(sizeof(byte *) * stc_vec_len_unsafe(prog->insts));
    m[0]       = malloc(sizeof(byte) * len);
    memset((void *) m[0], 0, sizeof(byte) * len);

    for (i = 1; i < stc_vec_len_unsafe(prog->insts); i++) {
        m[i] = malloc(sizeof(byte) * len);
        memset((void *) m[i], 0, sizeof(byte) * len);
    }

    return m;
}

// XXX: assumes m, m[i] nonnull
static void destroy_memoised(byte **m, len_t insts_len)
{
    len_t i;

    for (i = 0; i < insts_len; i++) { free(m[i]); }
    free(m);
}

static int srvm_run(const char    *text,
                    ThreadManager *thread_manager,
                    Scheduler     *scheduler,
                    const char   **captures)
{
    void          *null    = NULL;
    int            matched = 0, cond;
    const Program *prog    = scheduler_program(scheduler);
    void          *thread, *t;
    const byte    *pc;
    byte         **memoised = init_memoised(prog, text);
    const char    *sp, *codepoint;
    len_t          ncaptures, intervals_len, k;
    offset_t       x, y;
    cntr_t         cval, n;
    Interval      *intervals;
    Scheduler     *s;

#define CURR_INSTR (uint) * (pc - 1)
    size_t instr_counts[NBYTECODES] = { 0 };

    while ((thread = scheduler_next(scheduler))) {
        if ((sp = thread_manager->sp(thread)) != text && sp[-1] == '\0') {
            scheduler_kill(scheduler, thread);
            continue;
        }

        pc = thread_manager->pc(thread);
        switch (*pc++) {
            case NOOP: /* fallthrough */
                instr_counts[CURR_INSTR]++;
                thread_manager->set_pc(thread, pc);
                scheduler_schedule(scheduler, thread);

            case MATCH:
                instr_counts[CURR_INSTR]++;
                scheduler_notify_match(scheduler);
                matched = 1;
                if (captures)
                    memcpy(captures,
                           thread_manager->captures(thread, &ncaptures),
                           2 * ncaptures * sizeof(char *));
                thread_manager->free(thread);
                break;

            case BEGIN:
                instr_counts[CURR_INSTR]++;
                if (sp == text) {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case END:
                instr_counts[CURR_INSTR]++;
                if (*sp == '\0') {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case MEMO:
                if (memoised[pc - prog->insts][sp - text]) {
                    scheduler_kill(scheduler, thread); // TODO
                } else {
                    instr_counts[CURR_INSTR]++;
                    memoised[pc - prog->insts][sp - text] = (byte) 1;
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                }
                break;

            case CHAR: /* fallthrough */
                instr_counts[CURR_INSTR]++;
                MEMREAD(codepoint, pc, const char *);
                if (*sp && stc_utf8_cmp(codepoint, sp) == 0) {
                    thread_manager->set_pc(thread, pc);
                    thread_manager->try_inc_sp(thread);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case PRED: /* fallthrough */
                instr_counts[CURR_INSTR]++;
                MEMREAD(intervals_len, pc, len_t);
                MEMREAD(k, pc, len_t);
                intervals = (Interval *) (prog->aux + k);
                if (*sp && intervals_predicate(intervals, intervals_len, sp)) {
                    thread_manager->set_pc(thread, pc);
                    thread_manager->try_inc_sp(thread);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case SAVE:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->set_capture(thread, k);
                scheduler_schedule(scheduler, thread);
                break;

            case JMP:
                instr_counts[CURR_INSTR]++;
                MEMREAD(x, pc, offset_t);
                thread_manager->set_pc(thread, pc + x);
                scheduler_schedule(scheduler, thread);
                break;

            case SPLIT:
                instr_counts[CURR_INSTR]++;
                t = thread_manager->clone(thread);
                MEMREAD(x, pc, offset_t);
                thread_manager->set_pc(thread, pc + x);
                MEMREAD(y, pc, offset_t);
                thread_manager->set_pc(t, pc + y);
                scheduler_schedule(scheduler, thread);
                scheduler_schedule(scheduler, t);
                break;

            /* TODO: */
            case GSPLIT: break;
            case LSPLIT: break;

            case TSWITCH:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                for (; k > 0; k--) {
                    MEMREAD(x, pc, offset_t);
                    t = thread_manager->clone(thread);
                    thread_manager->set_pc(t, pc + x);
                    scheduler_schedule_in_order(scheduler, t);
                }
                thread_manager->free(thread);
                break;

            case EPSRESET:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->set_memory(thread, k, &null, sizeof(null));
                scheduler_schedule(scheduler, thread);
                break;

            case EPSSET:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->set_memory(thread, k, &sp, sizeof(sp));
                scheduler_schedule(scheduler, thread);
                break;

            case EPSCHK:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                if (*(char **) thread_manager->memory(thread, k) < sp) {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case RESET:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                MEMREAD(cval, pc, cntr_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->set_counter(thread, k, cval);
                scheduler_schedule(scheduler, thread);
                break;

            case CMP:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                MEMREAD(n, pc, cntr_t);
                cval = thread_manager->counter(thread, k);
                switch (*pc++) {
                    case LT: cond = (cval < n); break;
                    case LE: cond = (cval <= n); break;
                    case EQ: cond = (cval == n); break;
                    case NE: cond = (cval != n); break;
                    case GE: cond = (cval >= n); break;
                    case GT: cond = (cval > n); break;
                    default: cond = 0; break;
                }

                if (cond) {
                    thread_manager->set_pc(thread, pc);
                    scheduler_schedule(scheduler, thread);
                } else {
                    scheduler_kill(scheduler, thread);
                }
                break;

            case INC:
                instr_counts[CURR_INSTR]++;
                MEMREAD(k, pc, len_t);
                thread_manager->set_pc(thread, pc);
                thread_manager->inc_counter(thread, k);
                scheduler_schedule(scheduler, thread);
                break;

            case ZWA:
                instr_counts[CURR_INSTR]++;
                t = thread_manager->clone(thread);
                MEMREAD(x, pc, offset_t);
                thread_manager->set_pc(t, pc + x);
                MEMREAD(y, pc, offset_t);
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

            default: assert(0 && "unreachable");
        }
    }

    destroy_memoised(memoised, stc_vec_len_unsafe(prog->insts));

#define LOG_INSTRS(i) printf(#i ": %lu\n", instr_counts[(uint) i])

    LOG_INSTRS(CHAR);
    LOG_INSTRS(MEMO);

#undef LOG_INSTRS

    return matched;
}
