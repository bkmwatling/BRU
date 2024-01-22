#include <assert.h>
#include <string.h>

#include "stc/fatp/vec.h"

#include "program.h"
#include "sre.h"
#include "thompson.h"

#define PC(insts) (insts + stc_vec_len_unsafe(insts))

#define SET_OFFSET(p, pc) (*(p) = (pc) - (byte *) ((p) + 1))

#define SPLIT_LABELS_PTRS(p, q, re, insts)                             \
    do {                                                               \
        (p) = (offset_t *) ((re)->pos ? PC(insts)                      \
                                      : PC(insts) + sizeof(offset_t)); \
        (q) = (offset_t *) ((re)->pos ? PC(insts) + sizeof(offset_t)   \
                                      : PC(insts));                    \
    } while (0)

static len_t count(const Regex        *re,
                   len_t              *aux_len,
                   len_t              *ncaptures,
                   len_t              *ncounters,
                   len_t              *mem_len,
                   const CompilerOpts *opts);
static void  emit(const Regex *re, Program *prog, const CompilerOpts *opts);

const Program *thompson_compile(const Regex *re, const CompilerOpts *opts)
{
    len_t    insts_len, aux_len = 0, ncaptures = 0, ncounters = 0, mem_len = 0;
    Program *prog;

    insts_len = count(re, &aux_len, &ncaptures, &ncounters, &mem_len, opts) + 1;
    prog      = program_new(insts_len, aux_len, ncounters, mem_len, ncaptures);

    emit(re, prog, opts);
    BCWRITE(prog->insts, MATCH);

    return prog;
}

static len_t count(const Regex        *re,
                   len_t              *aux_len,
                   len_t              *ncaptures,
                   len_t              *ncounters,
                   len_t              *mem_len,
                   const CompilerOpts *opts)
{
    len_t n = 0;

    switch (re->type) {
        case CARET:   /* fallthrough */
        case MEMOISE: /* fallthrough */
        case DOLLAR: n = sizeof(byte); break;

        case LITERAL: n = sizeof(byte) + sizeof(const char *); break;

        case CC:
            n         = sizeof(byte) + 2 * sizeof(len_t);
            *aux_len += re->cc_len * sizeof(Interval);
            break;

        case ALT:
            n  = 2 * sizeof(byte) + 3 * sizeof(offset_t);
            n += count(re->left, aux_len, ncaptures, ncounters, mem_len, opts) +
                 count(re->right, aux_len, ncaptures, ncounters, mem_len, opts);
            break;

        case CONCAT:
            n = count(re->left, aux_len, ncaptures, ncounters, mem_len, opts) +
                count(re->right, aux_len, ncaptures, ncounters, mem_len, opts);
            break;

        case CAPTURE:
            n  = 2 * sizeof(byte) + 2 * sizeof(len_t);
            n += count(re->left, aux_len, ncaptures, ncounters, mem_len, opts);
            if (*ncaptures < re->capture_idx + 1)
                *ncaptures = re->capture_idx + 1;
            break;

        case STAR:
            if (opts->capture_semantics == CS_PCRE) {
                n = 5 * sizeof(byte) + 5 * sizeof(offset_t) + 2 * sizeof(len_t);
            } else if (opts->capture_semantics == CS_RE2) {
                n = 5 * sizeof(byte) + 5 * sizeof(offset_t) + 2 * sizeof(len_t);
            }
            n += count(re->left, aux_len, ncaptures, ncounters, mem_len, opts);
            *mem_len += sizeof(char *);
            break;

        case PLUS:
            if (opts->capture_semantics == CS_PCRE) {
                n = 4 * sizeof(byte) + 3 * sizeof(offset_t) + 2 * sizeof(len_t);
            } else if (opts->capture_semantics == CS_RE2) {
                n = 4 * sizeof(byte) + 3 * sizeof(offset_t) + 2 * sizeof(len_t);
            }
            n += count(re->left, aux_len, ncaptures, ncounters, mem_len, opts);
            *mem_len += sizeof(char *);
            break;

        case QUES:
            n  = sizeof(byte) + 2 * sizeof(offset_t);
            n += count(re->left, aux_len, ncaptures, ncounters, mem_len, opts);
            break;

        case COUNTER:
            if (opts->capture_semantics == CS_PCRE) {
                n = 11 * sizeof(byte) + 5 * sizeof(offset_t) +
                    6 * sizeof(len_t) + 3 * sizeof(cntr_t);
            } else if (opts->capture_semantics == CS_RE2) {
                n = 11 * sizeof(byte) + 5 * sizeof(offset_t) +
                    6 * sizeof(len_t) + 3 * sizeof(cntr_t);
            }
            n += count(re->left, aux_len, ncaptures, ncounters, mem_len, opts);
            ++*ncounters;
            *mem_len += sizeof(char *);
            break;

        case LOOKAHEAD:
            n  = 3 * sizeof(byte) + 2 * sizeof(offset_t);
            n += count(re->left, aux_len, ncaptures, ncounters, mem_len, opts);
            break;
        case NREGEXTYPES: assert(0 && "unreachable");
    }

    return n;
}

static void emit(const Regex *re, Program *prog, const CompilerOpts *opts)
{
    len_t     c, k;
    offset_t *p, *q, *x, *y;
    byte     *insts = prog->insts;

    switch (re->type) {
        case MEMOISE: BCWRITE(insts, MEMO); break;
        case CARET: BCWRITE(insts, BEGIN); break;
        case DOLLAR: BCWRITE(insts, END); break;

        /* `char` ch */
        case LITERAL:
            BCWRITE(insts, CHAR);
            MEMWRITE(insts, const char *, re->ch);
            break;

        /* `pred` l p */
        case CC:
            BCWRITE(insts, PRED);
            MEMWRITE(insts, len_t, re->cc_len);
            MEMWRITE(insts, len_t, stc_vec_len_unsafe(prog->aux));
            MEMCPY(prog->aux, re->intervals, re->cc_len * sizeof(Interval));
            break;

        /*     `split` L1, L2               *
         * L1: instructions for `re->left`  *
         *     `jmp` L3                     *
         * L2: instructions for `re->right` *
         * L3:                              */
        case ALT:
            BCWRITE(insts, SPLIT);
            p                          = (offset_t *) PC(insts);
            stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
            SET_OFFSET(p, PC(insts));
            ++p;
            emit(re->left, prog, opts);
            BCWRITE(insts, JMP);
            q                          = (offset_t *) PC(insts);
            stc_vec_len_unsafe(insts) += sizeof(offset_t);
            SET_OFFSET(p, PC(insts));
            emit(re->right, prog, opts);
            SET_OFFSET(q, PC(insts));
            break;

        /* instructions for `re->left`  *
         * instructions for `re->right` */
        case CONCAT:
            emit(re->left, prog, opts);
            emit(re->right, prog, opts);
            break;

        /* `save` k                    *
         * instructions for `re->left` *
         * `save` k + 1                */
        case CAPTURE:
            BCWRITE(insts, SAVE);
            k = re->capture_idx;
            MEMWRITE(insts, len_t, 2 * k);
            emit(re->left, prog, opts);
            BCWRITE(insts, SAVE);
            MEMWRITE(insts, len_t, 2 * k + 1);
            break;

        /*     `split` L1, L5                         -- L5, L1 if non-greedy *
         * L1:                       | `jmp` L3                               *
         * L2: `epsset` k                                                     *
         * L3: instructions for `re->left`                                    *
         *     `split` L4, L5        |                -- L5, L4 if non-greedy *
         * L4: `epschk` k                                                     *
         *     `jmp` L2              | `split` L2, L5 -- L5, L2 if non-greedy *
         * L5:                                                                */
        case STAR:
            k = stc_vec_len_unsafe(prog->memory);
            MEMWRITE(prog->memory, const char *, NULL);

            BCWRITE(insts, SPLIT);
            SPLIT_LABELS_PTRS(p, q, re, insts);
            stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
            SET_OFFSET(p, PC(insts));

            if (opts->capture_semantics == CS_RE2) {
                BCWRITE(insts, JMP);
                y                          = (offset_t *) PC(insts);
                stc_vec_len_unsafe(insts) += sizeof(offset_t);
            }

            p = (offset_t *) PC(insts);
            BCWRITE(insts, EPSSET);
            MEMWRITE(insts, len_t, k);

            if (opts->capture_semantics == CS_RE2) SET_OFFSET(y, PC(insts));

            emit(re->left, prog, opts);

            if (opts->capture_semantics == CS_PCRE) {
                BCWRITE(insts, SPLIT);
                SPLIT_LABELS_PTRS(x, y, re, insts);
                stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
                SET_OFFSET(x, PC(insts));
            }

            BCWRITE(insts, EPSCHK);
            MEMWRITE(insts, len_t, k);

            if (opts->capture_semantics == CS_PCRE) {
                BCWRITE(insts, JMP);
                MEMWRITE(insts, offset_t,
                         (byte *) p - (PC(insts) + sizeof(offset_t)));
            } else if (opts->capture_semantics == CS_RE2) {
                BCWRITE(insts, SPLIT);
                SPLIT_LABELS_PTRS(x, y, re, insts);
                stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
                SET_OFFSET(x, (byte *) p);
            }

            SET_OFFSET(q, PC(insts));
            SET_OFFSET(y, PC(insts));
            break;

        /*                           | `jmp` L2                               *
         * L1: `epsset` k                                                     *
         * L2: instructions for `re->left`                                    *
         *     `split` L3, L4        |                -- L4, L3 if non-greedy *
         * L3: `epschk` k                                                     *
         *     `jmp` L1              | `split` L1, L4 -- L4, L1 if non-greedy *
         * L4:                                                                */
        case PLUS:
            k = stc_vec_len_unsafe(prog->memory);
            MEMWRITE(prog->memory, const char *, NULL);

            if (opts->capture_semantics == CS_RE2) {
                BCWRITE(insts, JMP);
                y                          = (offset_t *) PC(insts);
                stc_vec_len_unsafe(insts) += sizeof(offset_t);
            }

            x = (offset_t *) PC(insts);
            BCWRITE(insts, EPSSET);
            MEMWRITE(insts, len_t, k);

            if (opts->capture_semantics == CS_RE2) SET_OFFSET(y, PC(insts));

            emit(re->left, prog, opts);

            if (opts->capture_semantics == CS_PCRE) {
                BCWRITE(insts, SPLIT);
                SPLIT_LABELS_PTRS(p, q, re, insts);
                stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
                SET_OFFSET(p, PC(insts));
            }

            BCWRITE(insts, EPSCHK);
            MEMWRITE(insts, len_t, k);

            if (opts->capture_semantics == CS_PCRE) {
                BCWRITE(insts, JMP);
                MEMWRITE(insts, offset_t,
                         (byte *) x - (PC(insts) + sizeof(offset_t)));
            } else if (opts->capture_semantics == CS_RE2) {
                BCWRITE(insts, SPLIT);
                SPLIT_LABELS_PTRS(p, q, re, insts);
                stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
                SET_OFFSET(p, (byte *) x);
            }

            SET_OFFSET(q, PC(insts));
            break;

        /*     `split` L1, L2              -- L2, L1 if non-greedy *
         * L1: instructions for `re->left`                         *
         * L2:                                                     */
        case QUES:
            BCWRITE(insts, SPLIT);
            SPLIT_LABELS_PTRS(p, q, re, insts);
            stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
            SET_OFFSET(p, PC(insts));
            emit(re->left, prog, opts);
            SET_OFFSET(q, PC(insts));
            break;

        /*     `reset` c, 0                                                   *
         *     `split` L1, L5                         -- L5, L1 if non-greedy *
         * L1:                       | `jmp` L3                               *
         * L2: `epsset` k                                                     *
         * L3: `cmplt` c, `max`                                               *
         *     instructions for `re->left`                                    *
         *     `inc` c                                                        *
         *     `split` L4, L5        |                -- L5, L4 if non-greedy *
         * L4: `epschk` k                                                     *
         *     `jmp` L2              | `split` L2, L5 -- L5, L2 if non-greedy *
         * L5: `cmpge` c, `min`                                               */
        case COUNTER:
            k = stc_vec_len_unsafe(prog->memory);
            MEMWRITE(prog->memory, const char *, NULL);

            BCWRITE(insts, RESET);
            c                 = stc_vec_len_unsafe(prog->counters)++;
            prog->counters[c] = 0;
            MEMWRITE(insts, len_t, c);
            MEMWRITE(insts, cntr_t, 0);

            BCWRITE(insts, SPLIT);
            SPLIT_LABELS_PTRS(p, q, re, insts);
            stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
            SET_OFFSET(p, PC(insts));

            if (opts->capture_semantics == CS_RE2) {
                BCWRITE(insts, JMP);
                y                          = (offset_t *) PC(insts);
                stc_vec_len_unsafe(insts) += sizeof(offset_t);
            }

            p = (offset_t *) PC(insts);
            BCWRITE(insts, EPSSET);
            MEMWRITE(insts, len_t, k);

            if (opts->capture_semantics == CS_RE2) SET_OFFSET(y, PC(insts));

            BCWRITE(insts, CMP);
            MEMWRITE(insts, len_t, c);
            MEMWRITE(insts, cntr_t, re->max);
            BCWRITE(insts, LT);

            emit(re->left, prog, opts);

            BCWRITE(insts, INC);
            MEMWRITE(insts, len_t, c);

            if (opts->capture_semantics == CS_PCRE) {
                BCWRITE(insts, SPLIT);
                SPLIT_LABELS_PTRS(x, y, re, insts);
                stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
                SET_OFFSET(x, PC(insts));
            }

            BCWRITE(insts, EPSCHK);
            MEMWRITE(insts, len_t, k);

            if (opts->capture_semantics == CS_PCRE) {
                BCWRITE(insts, JMP);
                MEMWRITE(insts, offset_t,
                         (byte *) p - (PC(insts) + sizeof(offset_t)));
            } else if (opts->capture_semantics == CS_RE2) {
                BCWRITE(insts, SPLIT);
                SPLIT_LABELS_PTRS(x, y, re, insts);
                stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
                SET_OFFSET(x, (byte *) p);
            }

            SET_OFFSET(q, PC(insts));
            SET_OFFSET(y, PC(insts));
            BCWRITE(insts, CMP);
            MEMWRITE(insts, len_t, c);
            MEMWRITE(insts, cntr_t, re->min);
            BCWRITE(insts, GE);
            break;

        /*     `zwa` L1, L2, `neg`         *
         * L1: instructions for `re->left` *
         *     `match`                     *
         * L2:                             */
        case LOOKAHEAD:
            BCWRITE(insts, ZWA);
            p                          = (offset_t *) PC(insts);
            stc_vec_len_unsafe(insts) += 2 * sizeof(offset_t);
            BCWRITE(insts, re->pos);
            SET_OFFSET(p, PC(insts));
            ++p;
            emit(re->left, prog, opts);
            BCWRITE(insts, MATCH);
            SET_OFFSET(p, PC(insts));
            break;

        case NREGEXTYPES: assert(0 && "unreachable");
    }
}
