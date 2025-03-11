#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <bru/vm/program.h>
#include <bru/vm/srvm.h>

/* --- Type definitions ----------------------------------------------------- */

struct bru_srvm {
    BruThreadManager *thread_manager; /**< the thread manager to execute with */
    const BruProgram *program;        /**< the program of the SRVM to execute */
    const char       *curr_sp;        /**< the SP to generate threads from    */
    int matching_finished;            /**< flag to indicate matching is done  */
};

/* --- Private function prototypes ------------------------------------------ */

static BruSRVMMatch *srvm_run(BruSRVM *self, const char *text);

/* --- API function definitions --------------------------------------------- */

BruSRVM *bru_srvm_new(BruThreadManager *thread_manager, const BruProgram *prog)
{
    BruSRVM *srvm = malloc(sizeof(*srvm));

    srvm->thread_manager    = thread_manager;
    srvm->program           = prog;
    srvm->curr_sp           = NULL;
    srvm->matching_finished = FALSE;

    return srvm;
}

void bru_srvm_free(BruSRVM *self)
{
    bru_thread_manager_kill(self->thread_manager);
    free(self);
}

BruSRVMMatch *bru_srvm_match(BruSRVM *self, const char *text)
{
    if (text == NULL) return NULL;

    self->curr_sp           = text;
    self->matching_finished = FALSE;
    bru_thread_manager_reset(self->thread_manager);
    return srvm_run(self, text);
}

void bru_srvm_match_free(BruSRVMMatch *self)
{
    free(self->captures);
    free(self->bytes);
    free(self);
}

BruSRVMMatch *bru_srvm_find(BruSRVM *self, const char *text)
{
    if (text == NULL) return NULL;

    if (self->curr_sp == NULL) {
        self->curr_sp           = text;
        self->matching_finished = FALSE;
        bru_thread_manager_reset(self->thread_manager);
    }

    return srvm_run(self, text);
}

int bru_srvm_matches(BruThreadManager *thread_manager,
                     const BruProgram *prog,
                     const char       *text)
{
    BruSRVM      *srvm    = bru_srvm_new(thread_manager, prog);
    BruSRVMMatch *match   = bru_srvm_match(srvm, text);
    int           matched = match != NULL;
    bru_srvm_free(srvm);
    bru_srvm_match_free(match);

    return matched;
}

/* --- Private function definitions ----------------------------------------- */

static BruSRVMMatch *srvm_match_from_thread(BruThreadManager *tm,
                                            BruThread        *thread)
{
    BruSRVMMatch      *match;
    const char *const *captures;
    bru_len_t          k;

    if (thread == NULL) return NULL;

    match        = calloc(1, sizeof(*match));
    match->bytes = bru_thread_manager_bytes(tm, thread, &match->nbytes);
    captures     = bru_thread_manager_captures(tm, thread, &match->ncaptures);
    match->captures = malloc(sizeof(*match->captures) * match->ncaptures);
    for (k = 0; k < match->ncaptures; k++)
        match->captures[k] =
            stc_sv_from_range(captures[2 * k], captures[2 * k + 1]);

    return match;
}

static BruSRVMMatch *srvm_run(BruSRVM *self, const char *text)
{
    void             *null = NULL;
    int               cond;
    const BruProgram *prog = self->program;
    BruThreadManager *tm   = self->thread_manager;
    BruThread        *thread, *t;
    const bru_byte_t *pc;
    const char       *sp, *codepoint, *matched_sp;
    const char       *capture_start, *capture_end;
    bru_len_t         k, l;
    bru_offset_t      x, y;
    bru_cntr_t        cval, n;
    bru_byte_t        byte;
    BruIntervals     *intervals;

    if (self->matching_finished) return FALSE;

    bru_thread_manager_init_memoisation(self->thread_manager,
                                        self->program->nmemo_insts, text);
    do {
        bru_thread_manager_init(tm, self->program->insts, self->curr_sp);
        while ((thread = bru_thread_manager_next_thread(tm))) {
            if ((sp = bru_thread_manager_sp(tm, thread)) > text &&
                sp[-1] == '\0') {
                bru_thread_manager_kill_thread(tm, thread);
                continue;
            }

            pc = bru_thread_manager_pc(tm, thread);
            switch (BRU_BCREAD(pc)) {
                case BRU_NOOP:
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_MATCH:
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

                case BRU_JMP:
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, thread, pc + x);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_SPLIT:
                    t = bru_thread_manager_clone_thread(tm, thread);
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
                        t = bru_thread_manager_clone_thread(tm, thread);
                        bru_thread_manager_set_pc(tm, t, pc + x);
                        bru_thread_manager_schedule_thread_in_order(tm, t);
                    }
                    // reuse current thread
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_thread_manager_set_pc(tm, thread, pc + x);
                    bru_thread_manager_schedule_thread_in_order(tm, thread);
                    break;

                case BRU_SAVE:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_set_capture(tm, thread, k);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_BACKREF:
                    // `k` is the capture group number
                    BRU_MEMREAD(k, pc, bru_len_t);
                    capture_start =
                        bru_thread_manager_capture_val(tm, thread, 2 * k);
                    capture_end =
                        bru_thread_manager_capture_val(tm, thread, 2 * k + 1);

                    // capture not used
                    if (!capture_start || !capture_end) goto backref_fail;

                    assert(capture_start <= capture_end);
                    // `l` is the capture length in bytes
                    l = capture_end - capture_start;

                    // empty capture; nothing to backref
                    if (l == 0) goto backref_finished;

                    // k is the number of bytes matched in this backref
                    k = bru_thread_manager_backref_index(tm, thread);
                    assert(l > k);

                    // backref still needs to match something
                    codepoint = capture_start + k;
                    if (*sp && stc_utf8_cmp(codepoint, sp) == 0) {
                        bru_thread_manager_inc_sp(tm, thread);
                        k += stc_utf8_nbytes(sp);
                        if (k == l) goto backref_finished;
                        bru_thread_manager_set_backref_index(tm, thread, k);
                        goto backref_done;
                    } else {
                        // `sp` did not match capture group
                        goto backref_fail;
                    }

                backref_finished:
                    bru_thread_manager_set_backref_index(tm, thread, 0);
                    bru_thread_manager_set_pc(tm, thread, pc);
                backref_done:
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;
                backref_fail:
                    bru_thread_manager_kill_thread(tm, thread);
                    break;

                case BRU_SET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    BRU_MEMREAD(cval, pc, bru_cntr_t);
                    bru_thread_manager_set_pc(tm, thread, pc);
                    bru_thread_manager_set_counter(tm, thread, k, cval);
                    bru_thread_manager_schedule_thread(tm, thread);
                    break;

                case BRU_CMP:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    BRU_MEMREAD(n, pc, bru_cntr_t);
                    cval = bru_thread_manager_counter(tm, thread, k);
                    switch ((BruOrd) *pc++) {
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
                    if (*(const char **) bru_thread_manager_memory(tm, thread,
                                                                   k) < sp) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_MEMOSET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_thread_manager_memoise_set(tm, thread, k);
                    bru_thread_manager_kill_thread(tm, thread);
                    break;

                case BRU_MEMOCHK:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    if (!bru_thread_manager_memoise_check(tm, thread, k)) {
                        bru_thread_manager_set_pc(tm, thread, pc);
                        bru_thread_manager_schedule_thread(tm, thread);
                    } else {
                        bru_thread_manager_kill_thread(tm, thread);
                    }
                    break;

                case BRU_ZWA:
                    t = bru_thread_manager_clone_thread(tm, thread);
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

        thread = bru_thread_manager_get_match(tm);
        if (bru_thread_manager_done_exec(tm)) {
            self->matching_finished = TRUE;
            break;
        }
        if (thread) matched_sp = bru_thread_manager_sp(tm, thread);
        self->curr_sp = thread && matched_sp > self->curr_sp
                            ? matched_sp
                            : stc_utf8_str_next(self->curr_sp);

    } while (thread == NULL);

    return srvm_match_from_thread(tm, thread);
}
