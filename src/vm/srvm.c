#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "program.h"
#include "srvm.h"

/* --- Type definitions ----------------------------------------------------- */

struct bru_srvm {
    BruThreadManager *thread_manager; /**< the thread manager to execute with */
    const BruProgram *program;        /**< the program of the SRVM to execute */
    const char       *curr_sp;        /**< the SP to generate threads from    */
    int          matching_finished;   /**< flag to indicate matching is done  */
    bru_len_t    ncaptures; /**< the number of captures in the program        */
    const char **captures;  /**< the array of (start, end) capture pairs      */
};

/* --- Private function prototypes ------------------------------------------ */

static int srvm_run(BruSRVM *self, const char *text);

/* --- API function definitions --------------------------------------------- */

BruSRVM *bru_srvm_new(BruThreadManager *thread_manager, const BruProgram *prog)
{
    BruSRVM *srvm = malloc(sizeof(*srvm));

    srvm->thread_manager    = thread_manager;
    srvm->program           = prog;
    srvm->curr_sp           = NULL;
    srvm->matching_finished = FALSE;
    srvm->ncaptures         = prog->ncaptures;
    srvm->captures          = malloc(2 * srvm->ncaptures * sizeof(char *));
    memset(srvm->captures, 0, 2 * srvm->ncaptures * sizeof(char *));

    return srvm;
}

void bru_srvm_free(BruSRVM *self)
{
    bru_thread_manager_free(self->thread_manager);
    free(self->captures);
    free(self);
}

int bru_srvm_match(BruSRVM *self, const char *text)
{
    if (text == NULL) return 0;

    self->curr_sp           = text;
    self->matching_finished = FALSE;
    memset(self->captures, 0, 2 * self->ncaptures * sizeof(char *));
    bru_thread_manager_reset(self->thread_manager);

    return srvm_run(self, text);
}

int bru_srvm_find(BruSRVM *self, const char *text)
{
    if (text == NULL) return 0;

    if (self->curr_sp == NULL) {
        self->curr_sp           = text;
        self->matching_finished = FALSE;
        bru_thread_manager_reset(self->thread_manager);
    }
    memset(self->captures, 0, 2 * self->ncaptures * sizeof(char *));

    return srvm_run(self, text);
}

StcStringView bru_srvm_capture(BruSRVM *self, bru_len_t idx)
{
    if (idx >= self->ncaptures) return (StcStringView) { 0, NULL };

    return stc_sv_from_range(self->captures[2 * idx],
                             self->captures[2 * idx + 1]);
}

StcStringView *bru_srvm_captures(BruSRVM *self, bru_len_t *ncaptures)
{
    bru_len_t      i;
    StcStringView *captures = malloc(self->ncaptures * sizeof(StcStringView));

    if (ncaptures) *ncaptures = self->ncaptures;
    for (i = 0; i < self->ncaptures; i++)
        captures[i] =
            stc_sv_from_range(self->captures[2 * i], self->captures[2 * i + 1]);

    return captures;
}

int bru_srvm_matches(BruThreadManager *thread_manager,
                     const BruProgram *prog,
                     const char       *text)
{
    BruSRVM *srvm    = bru_srvm_new(thread_manager, prog);
    int      matches = bru_srvm_match(srvm, text);
    bru_srvm_free(srvm);

    return matches;
}

/* --- Private function definitions ----------------------------------------- */

static int srvm_run(BruSRVM *self, const char *text)
{
    void              *null    = NULL;
    int                matched = FALSE, cond;
    const BruProgram  *prog    = self->program;
    BruThreadManager  *tm      = self->thread_manager;
    void              *thread, *t;
    const bru_byte_t  *pc;
    const char        *sp, *codepoint, *matched_sp;
    const char *const *captures;
    char             **epsset_marker;
    bru_len_t          ncaptures, k;
    bru_offset_t       x, y;
    bru_cntr_t         cval, n;
    bru_byte_t         byte;
    size_t             len;
    BruIntervals      *intervals;

    if (self->matching_finished) return FALSE;

    bru_thread_manager_init_memoisation(self->thread_manager,
                                        self->program->nmemo_insts, text);
    do {
        bru_thread_manager_init(tm, self->program->insts, self->curr_sp);
        while (bru_thread_manager_next_thread(tm, thread)) {
            if (bru_thread_manager_sp(tm, sp, thread) > text &&
                sp[-1] == '\0') {
                bru_thread_manager_kill_thread(tm, thread);
                continue;
            }

            bru_thread_manager_pc(tm, pc, thread);
            switch (*pc++) {
                case BRU_NOOP:
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_MATCH:
                    matched_sp = sp;
                    matched    = 1;
                    if (self->captures)
                        memcpy(self->captures,
                               bru_thread_manager_captures(tm, captures, thread,
                                                           &ncaptures),
                               2 * self->ncaptures * sizeof(char *));
                    bru_thread_manager_notify_thread_match(tm, thread);
                    break;

                case BRU_BEGIN:
                    if (sp == text) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_END:
                    if (*sp == '\0') {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_MEMO:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    if (bru_thread_manager_memoise(tm, cond, thread, k)) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_CHAR:
                    BRU_MEMREAD(codepoint, pc, const char *);
                    if (*sp && stc_utf8_cmp(codepoint, sp) == 0) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_inc_sp(tm, thread);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_PRED:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    intervals = (BruIntervals *) (prog->aux + k);
                    if (*sp && bru_intervals_predicate(intervals, sp)) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_inc_sp(tm, thread);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_SAVE:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_set_capture(tm, thread, k);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_JMP:
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, thread, pc + x);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_SPLIT:
                    bru_thread_manager_clone_thread(tm, t, thread);
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, thread, pc + x);
                    BRU_MEMREAD(y, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, t, pc + y);
                    bru_thread_manager_schedule_thread(tm, thread);
                    bru_thread_manager_schedule_thread(tm, t);
                    break;

                /* TODO: */
                case BRU_GSPLIT: break;
                case BRU_LSPLIT: break;

                case BRU_TSWITCH:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    // k > 1 to reuse current thread for last offset
                    for (; k > 1; k--) {
                        BRU_MEMREAD(x, pc, bru_offset_t);
                        bru_thread_manager_clone_thread(tm, t, thread);
                        bru_thread_manager_set_pc(tm, t, pc + x);
                        bru_thread_manager_schedule_thread_in_order(tm, t);
                    }
                    // reuse current thread
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, thread, pc + x);
                    bru_thread_manager_schedule_thread_in_order(tm, thread);
                    break;

                case BRU_EPSRESET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_set_memory(tm, thread, k, &null,
                                                  sizeof(null));
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_EPSSET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_set_memory(tm, thread, k, &sp,
                                                  sizeof(sp));
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_EPSCHK:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    if (*bru_thread_manager_memory(tm, epsset_marker, thread,
                                                   k) < sp) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_RESET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    BRU_MEMREAD(cval, pc, bru_cntr_t);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_set_counter(tm, thread, k, cval);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_CMP:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    BRU_MEMREAD(n, pc, bru_cntr_t);
                    bru_thread_manager_counter(tm, cval, thread, k);
                    switch (*pc++) {
                        case BRU_LT: cond = (cval < n); break;
                        case BRU_LE: cond = (cval <= n); break;
                        case BRU_EQ: cond = (cval == n); break;
                        case BRU_NE: cond = (cval != n); break;
                        case BRU_GE: cond = (cval >= n); break;
                        case BRU_GT: cond = (cval > n); break;
                        default: cond = 0; break;
                    }

                    if (cond) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_INC:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_inc_counter(tm, thread, k);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_ZWA:
                    bru_thread_manager_clone_thread(tm, t, thread);
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, t, pc + x);
                    BRU_MEMREAD(y, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, thread, pc + y);
                    // TODO:
                    // s = malloc(sizeof(*s));
                    // bru_scheduler_copy_with(s, scheduler, t);
                    //
                    // if (srvm_run(text, thread_manager, s, NULL) == *pc)
                    //     bru_scheduler_schedule(scheduler, thread);
                    // else
                    //     bru_scheduler_kill(scheduler, thread);
                    // bru_scheduler_free(s);
                    break;

                case BRU_STATE:
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_WRITE:
                    BRU_MEMREAD(byte, pc, bru_byte_t);
                    bru_thread_manager_write_byte(tm, thread, byte);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_WRITE0:
                    byte = '0';
                    bru_thread_manager_write_byte(tm, thread, byte);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_WRITE1:
                    byte = '1';
                    bru_thread_manager_write_byte(tm, thread, byte);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_NBYTECODES: assert(0 && "unreachable");
            }
        }

        if (bru_thread_manager_done_exec(tm, cond)) {
            self->matching_finished = TRUE;
            break;
        }
        self->curr_sp = matched && matched_sp != self->curr_sp
                            ? matched_sp
                            : stc_utf8_str_next(self->curr_sp);
    } while (!matched);

    return matched;
}
