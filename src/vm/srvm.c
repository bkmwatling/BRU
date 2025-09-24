#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <bru/vm/program.h>
#include <bru/vm/srvm.h>

/* --- Type definitions ----------------------------------------------------- */

struct bru_srvm {
    BruThreadManager *tm;            /**< the thread manager to execute with  */
    const BruProgram *program;       /**< the program of the SRVM to execute  */
    const char       *curr_sp;       /**< the SP to generate threads from     */
    bool              matching_done; /**< flag to indicate matching is done   */
};

/* --- Private function prototypes ------------------------------------------ */

static BruSRVMMatch *srvm_run(BruSRVM *self, const char *text);

/* --- API function definitions --------------------------------------------- */

BruSRVM *bru_srvm_new(BruThreadManager *tm, const BruProgram *prog)
{
    BruSRVM *srvm = malloc(sizeof(*srvm));

    srvm->tm            = tm;
    srvm->program       = prog;
    srvm->curr_sp       = NULL;
    srvm->matching_done = false;

    return srvm;
}

void bru_srvm_free(BruSRVM *self)
{
    bru_tm_kill(self->tm);
    free(self);
}

BruSRVMMatch *bru_srvm_match(BruSRVM *self, const char *text)
{
    if (text == NULL) return NULL;

    self->curr_sp       = text;
    self->matching_done = false;
    bru_tm_reset(self->tm);
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
        self->curr_sp       = text;
        self->matching_done = false;
        bru_tm_reset(self->tm);
    }

    return srvm_run(self, text);
}

bool bru_srvm_matches(BruThreadManager *tm,
                      const BruProgram *prog,
                      const char       *text)
{
    BruSRVM      *srvm    = bru_srvm_new(tm, prog);
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

    match           = calloc(1, sizeof(*match));
    match->bytes    = bru_tm_read_bytes(tm, thread, &match->nbytes);
    captures        = bru_tm_get_captures(tm, thread, &match->ncaptures);
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
    BruThreadManager *tm   = self->tm;
    BruThread        *thread, *t;
    const bru_byte_t *pc;
    const char       *sp, *codepoint, *matched_sp;
    const char       *capture_start, *capture_end;
    bru_len_t         k, l;
    bru_offset_t      x, y;
    bru_cntr_t        cval, n;
    bru_byte_t        byte;
    BruIntervals     *intervals;

    if (self->matching_done) return false;

    bru_tm_init_memoisation(self->tm, self->program->nmemo_insts, text);
    do {
        bru_tm_init(tm, self->program->insts, self->curr_sp);
        while ((thread = bru_tm_next_thread(tm))) {
            if ((sp = bru_tm_sp(tm, thread)) > text && sp[-1] == '\0') {
                bru_tm_kill_thread(tm, thread);
                continue;
            }

            pc = bru_tm_pc(tm, thread);
            switch (BRU_BCREAD(pc)) {
                case BRU_NOOP:
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_MATCH: bru_tm_notify_thread_match(tm, thread); break;

                case BRU_BEGIN:
                    if (sp == text) {
                        bru_tm_set_pc(tm, thread, pc);
                        bru_tm_schedule_thread(tm, thread);
                    } else {
                        bru_tm_kill_thread(tm, thread);
                    }
                    break;

                case BRU_END:
                    if (*sp == '\0') {
                        bru_tm_set_pc(tm, thread, pc);
                        bru_tm_schedule_thread(tm, thread);
                    } else {
                        bru_tm_kill_thread(tm, thread);
                    }
                    break;

                case BRU_CHAR:
                    BRU_MEMREAD(codepoint, pc, const char *);
                    if (*sp && stc_utf8_cmp(codepoint, sp) == 0) {
                        bru_tm_set_pc(tm, thread, pc);
                        bru_tm_inc_sp(tm, thread);
                        bru_tm_schedule_thread(tm, thread);
                    } else {
                        bru_tm_kill_thread(tm, thread);
                    }
                    break;

                case BRU_PRED:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    intervals = (BruIntervals *) (prog->aux + k);
                    if (*sp && bru_intervals_predicate(intervals, sp)) {
                        bru_tm_set_pc(tm, thread, pc);
                        bru_tm_inc_sp(tm, thread);
                        bru_tm_schedule_thread(tm, thread);
                    } else {
                        bru_tm_kill_thread(tm, thread);
                    }
                    break;

                case BRU_JMP:
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_tm_set_pc(tm, thread, pc + x);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_SPLIT:
                    t = bru_tm_clone_thread(tm, thread);
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_tm_set_pc(tm, thread, pc + x);
                    BRU_MEMREAD(y, pc, bru_offset_t);
                    bru_tm_set_pc(tm, t, pc + y);
                    bru_tm_schedule_thread(tm, thread);
                    bru_tm_schedule_thread(tm, t);
                    break;

                /* TODO: */
                case BRU_GSPLIT: break;
                case BRU_LSPLIT: break;

                case BRU_TSWITCH:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    // k > 1 to reuse current thread for last offset
                    for (; k > 1; k--) {
                        BRU_MEMREAD(x, pc, bru_offset_t);
                        t = bru_tm_clone_thread(tm, thread);
                        bru_tm_set_pc(tm, t, pc + x);
                        bru_tm_schedule_thread_in_order(tm, t);
                    }
                    // reuse current thread
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_tm_set_pc(tm, thread, pc + x);
                    bru_tm_schedule_thread_in_order(tm, thread);
                    break;

                case BRU_SAVE:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_set_capture(tm, thread, k);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_BACKREF:
                    // `k` is the capture group number
                    BRU_MEMREAD(k, pc, bru_len_t);
                    capture_start = bru_tm_get_capture(tm, thread, 2 * k);
                    capture_end   = bru_tm_get_capture(tm, thread, 2 * k + 1);

                    // capture not used
                    if (!capture_start || !capture_end) goto backref_fail;

                    assert(capture_start <= capture_end);
                    // `l` is the capture length in bytes
                    l = capture_end - capture_start;

                    // empty capture; nothing to backref
                    if (l == 0) goto backref_finished;

                    // k is the number of bytes matched in this backref
                    k = bru_tm_get_backref_index(tm, thread);
                    assert(l > k);

                    // backref still needs to match something
                    codepoint = capture_start + k;
                    if (*sp && stc_utf8_cmp(codepoint, sp) == 0) {
                        bru_tm_inc_sp(tm, thread);
                        k += stc_utf8_nbytes(sp);
                        if (k == l) goto backref_finished;
                        bru_tm_set_backref_index(tm, thread, k);
                        goto backref_done;
                    } else {
                        // `sp` did not match capture group
                        goto backref_fail;
                    }

                backref_finished:
                    bru_tm_set_backref_index(tm, thread, 0);
                    bru_tm_set_pc(tm, thread, pc);
                backref_done:
                    bru_tm_schedule_thread(tm, thread);
                    break;
                backref_fail:
                    bru_tm_kill_thread(tm, thread);
                    break;

                case BRU_SET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    BRU_MEMREAD(cval, pc, bru_cntr_t);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_set_counter(tm, thread, k, cval);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_CMP:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    BRU_MEMREAD(n, pc, bru_cntr_t);
                    cval = bru_tm_get_counter(tm, thread, k);
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
                        bru_tm_set_pc(tm, thread, pc);
                        bru_tm_schedule_thread(tm, thread);
                    } else {
                        bru_tm_kill_thread(tm, thread);
                    }
                    break;

                case BRU_INC:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_inc_counter(tm, thread, k);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_EPSRESET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_set_memory(tm, thread, k, &null, sizeof(null));
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_EPSSET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_set_memory(tm, thread, k, &sp, sizeof(sp));
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_EPSCHK:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    if (*(char **) bru_tm_get_memory(tm, thread, k) < sp) {
                        bru_tm_set_pc(tm, thread, pc);
                        bru_tm_schedule_thread(tm, thread);
                    } else {
                        bru_tm_kill_thread(tm, thread);
                    }
                    break;

                case BRU_MEMOSET:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    bru_tm_memoise_set(tm, thread, k);
                    bru_tm_kill_thread(tm, thread);
                    break;

                case BRU_MEMOCHK:
                    BRU_MEMREAD(k, pc, bru_len_t);
                    if (!bru_tm_memoise_check(tm, thread, k)) {
                        bru_tm_set_pc(tm, thread, pc);
                        bru_tm_schedule_thread(tm, thread);
                    } else {
                        bru_tm_kill_thread(tm, thread);
                    }
                    break;

                case BRU_ZWA:
                    t = bru_tm_clone_thread(tm, thread);
                    BRU_MEMREAD(x, pc, bru_offset_t);
                    bru_tm_set_pc(tm, t, pc + x);
                    BRU_MEMREAD(y, pc, bru_offset_t);
                    bru_tm_set_pc(tm, thread, pc + y);
                    // TODO:
                    // s = malloc(sizeof(*s));
                    // bru_tm_copy_with(s, scheduler, t);
                    //
                    // if (srvm_run(text, tm, s, NULL) == *pc)
                    //     bru_tm_schedule(scheduler, thread);
                    // else
                    //     bru_tm_kill(scheduler, thread);
                    // bru_tm_free(s);
                    break;

                case BRU_STATE:
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_WRITE:
                    BRU_MEMREAD(byte, pc, bru_byte_t);
                    bru_tm_write_byte(tm, thread, byte);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_WRITE0:
                    byte = '0';
                    bru_tm_write_byte(tm, thread, byte);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_WRITE1:
                    byte = '1';
                    bru_tm_write_byte(tm, thread, byte);
                    bru_tm_set_pc(tm, thread, pc);
                    bru_tm_schedule_thread(tm, thread);
                    break;

                case BRU_NBYTECODES: assert(false && "unreachable");
            }
        }

        thread = bru_tm_get_match(tm);
        if (bru_tm_done_exec(tm)) {
            self->matching_done = true;
            break;
        }
        if (thread) matched_sp = bru_tm_sp(tm, thread);
        self->curr_sp = thread && matched_sp > self->curr_sp
                            ? matched_sp
                            : stc_utf8_str_next(self->curr_sp);

    } while (thread == NULL);

    return srvm_match_from_thread(tm, thread);
}
