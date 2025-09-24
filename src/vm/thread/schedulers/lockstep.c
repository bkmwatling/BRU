#include <stdbool.h>
#include <stdlib.h>

#include <bru/vm/thread/schedulers/lockstep.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    BruThreadManager *tm; /**< the thread manager using this scheduler        */
    bool   in_lockstep;   /**< whether to execute the sync thread queue       */
    size_t curr_idx;      /**< index into current queue for next_thread       */

    StcVec(BruThread *) curr; /**< current queue of threads to execute        */
    StcVec(BruThread *) next; /**< next queue of threads to be executed       */
    StcVec(BruThread *) sync; /**< synchronisation queue for lockstep         */
} BruLockstepThreadScheduler;

/* --- LockstepScheduler function prototypes -------------------------------- */

static void       lockstep_ts_init(void *impl);
static bool       lockstep_ts_schedule(void *impl, BruThread *thread);
static bool       lockstep_ts_has_next(const void *impl);
static BruThread *lockstep_ts_next(void *impl);
static void       lockstep_ts_free(void *impl);

/* --- Helper function prototypes ------------------------------------------- */

static bool lockstep_threads_contain(BruThreadManager   *tm,
                                     StcVec(BruThread *) threads,
                                     BruThread          *thread);

/* --- Lockstep function prototypes ----------------------------------------- */

BruThreadScheduler *bru_lockstep_ts_new(BruThreadManager *tm)
{
    BruLockstepThreadScheduler *ls = malloc(sizeof(*ls));
    BruThreadScheduler         *s  = malloc(sizeof(*s));

    ls->tm          = tm;
    ls->in_lockstep = false;
    ls->curr_idx    = 0;
    stc_vec_default_init(&ls->curr);
    stc_vec_default_init(&ls->next);
    stc_vec_default_init(&ls->sync);

    s->impl              = ls;
    s->init              = lockstep_ts_init;
    s->schedule          = lockstep_ts_schedule;
    s->schedule_in_order = lockstep_ts_schedule;
    s->has_next          = lockstep_ts_has_next;
    s->next              = lockstep_ts_next;
    s->free              = lockstep_ts_free;

    return s;
}

StcVec(BruThread *)
bru_lockstep_ts_remove_low_priority_threads(BruThreadScheduler *self)
{
    BruLockstepThreadScheduler *ls      = self->impl;
    StcVec(BruThread *)         threads = NULL;
    size_t                      ncurr   = stc_vec_len(ls->curr) - ls->curr_idx;
    size_t                      i;

    if (ncurr) {
        stc_vec_init(&threads, ncurr);
        for (i = 0; i < ncurr; i++)
            stc_vec_push_back(&threads, stc_vec_pop_back(&ls->curr));
    }

    return threads;
}

bool bru_lockstep_ts_done_step(BruThreadScheduler *self)
{
    BruLockstepThreadScheduler *ls = self->impl;
    return ls->curr_idx >= stc_vec_len(ls->curr) && ls->in_lockstep;
}

/* --- LockstepScheduler function definitions ------------------------------- */

static void lockstep_ts_init(void *impl)
{
    BruLockstepThreadScheduler *self = impl;

    self->in_lockstep = false;
    self->curr_idx    = 0;
}

static void lockstep_schedule_char_match_instr(BruLockstepThreadScheduler *self,
                                               BruThread *thread)
{

    if (stc_vec_is_empty(self->next))
        stc_vec_push_back(&self->sync, thread);
    else
        stc_vec_push_back(&self->next, thread);
}

static void
lockstep_schedule_non_char_match_instr(BruLockstepThreadScheduler *self,
                                       BruThread                  *thread)
{
    stc_vec_push_back(&self->next, thread);
}

static bool lockstep_ts_schedule(void *impl, BruThread *thread)
{
    BruLockstepThreadScheduler *self = impl;
    bru_len_t                   k;
    const char                 *capture_start, *capture_end;

    if (lockstep_threads_contain(self->tm, self->next, thread) ||
        lockstep_threads_contain(self->tm, self->sync, thread))
        return false;

    switch ((BruBytecode) *bru_tm_pc(self->tm, thread)) {
        case BRU_CHAR: /* fallthrough */
        case BRU_PRED: lockstep_schedule_char_match_instr(self, thread); break;

        case BRU_BACKREF:
            k             = bru_tm_get_backref_index(self->tm, thread);
            capture_start = bru_tm_get_capture(self->tm, thread, 2 * k);
            capture_end   = bru_tm_get_capture(self->tm, thread, 2 * k + 1);

            if (!capture_start || !capture_end ||
                capture_end - capture_start == 0) {
                // either the capture was not used, or it matched the empty
                // string
                lockstep_schedule_non_char_match_instr(self, thread);
            } else {
                // otherwise, backref will try match a character in the
                // non-empty capture
                lockstep_schedule_char_match_instr(self, thread);
            }
            break;

        case BRU_NOOP:       /* fallthrough */
        case BRU_MATCH:      /* fallthrough */
        case BRU_BEGIN:      /* fallthrough */
        case BRU_END:        /* fallthrough */
        case BRU_JMP:        /* fallthrough */
        case BRU_SPLIT:      /* fallthrough */
        case BRU_GSPLIT:     /* fallthrough */
        case BRU_LSPLIT:     /* fallthrough */
        case BRU_TSWITCH:    /* fallthrough */
        case BRU_SAVE:       /* fallthrough */
        case BRU_INC:        /* fallthrough */
        case BRU_SET:        /* fallthrough */
        case BRU_CMP:        /* fallthrough */
        case BRU_EPSRESET:   /* fallthrough */
        case BRU_EPSSET:     /* fallthrough */
        case BRU_EPSCHK:     /* fallthrough */
        case BRU_MEMOSET:    /* fallthrough */
        case BRU_MEMOCHK:    /* fallthrough */
        case BRU_ZWA:        /* fallthrough */
        case BRU_STATE:      /* fallthrough */
        case BRU_WRITE:      /* fallthrough */
        case BRU_WRITE0:     /* fallthrough */
        case BRU_WRITE1:     /* fallthrough */
        case BRU_NBYTECODES: /* fallthrough */
        default: lockstep_schedule_non_char_match_instr(self, thread); break;
    }
    return true;
}

static bool lockstep_ts_has_next(const void *impl)
{
    const BruLockstepThreadScheduler *self = impl;
    return self->curr_idx < stc_vec_len(self->curr) ||
           !(stc_vec_is_empty(self->next) && stc_vec_is_empty(self->sync));
}

static BruThread *lockstep_ts_next(void *impl)
{
    BruLockstepThreadScheduler *self   = impl;
    BruThread                  *thread = NULL;
    BruThread                 **tmp;

lockstep_ts_next_start:
    if (self->curr_idx >= stc_vec_len(self->curr)) {
        self->curr_idx = 0;
        stc_vec_clear(&self->curr);
        if (stc_vec_is_empty(self->next)) {
            self->in_lockstep = true;
            tmp               = self->curr;
            self->curr        = self->sync;
            self->sync        = tmp;
        } else {
            self->in_lockstep = false;
            tmp               = self->curr;
            self->curr        = self->next;
            self->next        = tmp;
        }
    }

    if (self->curr_idx < stc_vec_len(self->curr)) {
        thread = self->curr[self->curr_idx++];
        switch ((BruBytecode) *bru_tm_pc(self->tm, thread)) {
            case BRU_CHAR: /* fallthrough */
            case BRU_PRED:
                if (!self->in_lockstep) {
                    if (!lockstep_ts_schedule(self, thread))
                        bru_tm_kill_thread(self->tm, thread);
                    goto lockstep_ts_next_start;
                }
                break;

            case BRU_NOOP:       /* fallthrough */
            case BRU_MATCH:      /* fallthrough */
            case BRU_BEGIN:      /* fallthrough */
            case BRU_END:        /* fallthrough */
            case BRU_JMP:        /* fallthrough */
            case BRU_SPLIT:      /* fallthrough */
            case BRU_GSPLIT:     /* fallthrough */
            case BRU_LSPLIT:     /* fallthrough */
            case BRU_TSWITCH:    /* fallthrough */
            case BRU_SAVE:       /* fallthrough */
            case BRU_BACKREF:    /* fallthrough */
            case BRU_INC:        /* fallthrough */
            case BRU_SET:        /* fallthrough */
            case BRU_CMP:        /* fallthrough */
            case BRU_EPSRESET:   /* fallthrough */
            case BRU_EPSSET:     /* fallthrough */
            case BRU_EPSCHK:     /* fallthrough */
            case BRU_MEMOSET:    /* fallthrough */
            case BRU_MEMOCHK:    /* fallthrough */
            case BRU_ZWA:        /* fallthrough */
            case BRU_STATE:      /* fallthrough */
            case BRU_WRITE:      /* fallthrough */
            case BRU_WRITE0:     /* fallthrough */
            case BRU_WRITE1:     /* fallthrough */
            case BRU_NBYTECODES: /* fallthrough */
            default: break;
        }
    }

    return thread;
}

static void lockstep_ts_free(void *impl)
{
    BruLockstepThreadScheduler *self = impl;
    stc_vec_free(self->curr);
    stc_vec_free(self->next);
    stc_vec_free(self->sync);
    free(self);
}

static bool lockstep_threads_contain(BruThreadManager   *tm,
                                     StcVec(BruThread *) threads,
                                     BruThread          *thread)
{
    size_t i, len;

    len = stc_vec_len(threads);
    for (i = 0; i < len; i++)
        if (bru_tm_check_thread_eq(tm, threads[i], thread)) return true;

    return false;
}
