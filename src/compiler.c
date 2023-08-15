#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

len_t count(Regex *re,
            len_t *aux_size,
            len_t *grp_cnt,
            len_t *counters_len,
            len_t *mem_len)
{
    len_t n = 0;

    switch (re->type) {
        case CARET:
        case DOLLAR: n = sizeof(byte); break;

        case LITERAL:
            n          = sizeof(byte) + sizeof(len_t) + sizeof(Interval *);
            *aux_size += sizeof(Interval);
            break;

        case CC:
            n          = sizeof(byte) + sizeof(len_t) + sizeof(Interval *);
            *aux_size += re->cc_len * sizeof(Interval);
            break;

        case ALT:
            n  = 2 * sizeof(byte) + 3 * sizeof(Inst *);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len) +
                 count(re->right, aux_size, grp_cnt, counters_len, mem_len);
            break;

        case CONCAT:
            n = count(re->left, aux_size, grp_cnt, counters_len, mem_len) +
                count(re->right, aux_size, grp_cnt, counters_len, mem_len);
            break;

        case CAPTURE:
            n  = 2 * sizeof(byte) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            ++*grp_cnt;
            break;

        case STAR:
            n  = 5 * sizeof(byte) + 5 * sizeof(Inst *) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            ++*mem_len;
            break;

        case PLUS:
            n  = 4 * sizeof(byte) + 3 * sizeof(Inst *) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            ++*mem_len;
            break;

        case QUES:
            n  = sizeof(byte) + 2 * sizeof(Inst *);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            break;

        case COUNTER:
            n = 9 * sizeof(byte) + 5 * sizeof(Inst *) + 6 * sizeof(len_t) +
                3 * sizeof(cntr_t) + 2 * sizeof(Order);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            ++*counters_len;
            ++*mem_len;
            break;

        case LOOKAHEAD:
            n  = 2 * sizeof(byte) + 2 * sizeof(Inst *) + sizeof(int);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            break;
    }

    return n;
}

byte *emit(Regex *re, byte *pc, Program *prog)
{
    len_t tmp;

    switch (re->type) {
        case CARET: *pc++ = BEGIN; break;
        case DOLLAR: *pc++ = END; break;

        case LITERAL: *pc++ = PRED; break;

        case CC: *pc++ = PRED; break;

        /*     split L1, L2                 *
         * L1: instructions for `re->left`  *
         *     `jmp` L3                     *
         * L2: instructions for `re->right` *
         * L3:                              */
        case ALT: *pc++ = SPLIT; break;

        /* instructions for `re->left`  *
         * instructions for `re->right` */
        case CONCAT:
            pc = emit(re->left, pc, prog);
            pc = emit(re->right, pc, prog);
            break;

        /* `save` k                    *
         * instructions for `re->left` *
         * `save` k + 1                */
        case CAPTURE:
            tmp   = prog->grp_cnt++;
            *pc++ = SAVE;
            MEMSET(pc, len_t, tmp);
            pc    = emit(re, pc, prog);
            *pc++ = SAVE;
            MEMSET(pc, len_t, tmp + 1);
            break;

        /*     `split` L1, L3              -- L3, L1 if non-greedy *
         * L1: `epsset` k                                          *
         *     instructions for `re->left`                         *
         *     `split` L2, L3              -- L3, L2 if non-greedy *
         * L2: `epschk` k                                          *
         *     `jmp` L1                                            *
         * L3:                                                     */
        case STAR: *pc++ = SPLIT; break;

        /* L1: `epsset` k                                          *
         *     instructions for `re->left`                         *
         *     `split` L2, L3              -- L3, L2 if non-greedy *
         * L2: `epschk` k                                          *
         *     `jmp` L1                                            *
         * L3:                                                     */
        case PLUS: *pc++ = EPSSET; break;

        /*     `split` L1, L2              -- L2, L1 if non-greedy *
         * L1: instructions for `re->left`                         *
         * L2:                                                     */
        case QUES: *pc++ = SPLIT; break;

        /*     `reset` c, 0                                        *
         *     `split` L1, L3              -- L3, L1 if non-greedy *
         * L1: `cmplt` c, `max`                                    *
         *     `epsset` k                                          *
         *     instructions for `re->left`                         *
         *     `inc` c                                             *
         *     `split` L2, L3              -- L3, L2 if non-greedy *
         * L2: `epschk` k                                          *
         *     `jmp` L1                                            *
         * L3: `cmpge` c, `min`                                    */
        case COUNTER: *pc++ = RESET; break;

        /*     `zwa` L1, L2, `neg`         *
         * L1: instructions for `re->left` *
         *     `match`                     *
         * L2:                             */
        case LOOKAHEAD: *pc++ = ZWA; break;
    }

    return pc;
}

Program *compile(Compiler *compiler)
{
    len_t insts_size, aux_size = 0, grp_cnt = 0, counters_len = 0, mem_len = 0;
    Program *prog;
    Regex   *re;

    re         = parse(compiler->parser);
    insts_size = count(re, &aux_size, &grp_cnt, &counters_len, &mem_len);
    prog       = program(insts_size, aux_size, grp_cnt, counters_len, mem_len);

    /* set the length fields to 0 as we use them for indices during emitting */
    prog->counters_len = 0;
    prog->mem_len      = 0;
    emit(re, prog->insts, prog);

    regex_free(re);

    return NULL;
}

void compiler_free(Compiler *compiler) { free(compiler->parser); }
