#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "sre.h"

#define SPLIT_LABELS_PTRS(p, q, re, pc)                              \
    (p) = (offset_t *) ((re)->pos ? (pc) : (pc) + sizeof(offset_t)); \
    (q) = (offset_t *) ((re)->pos ? (pc) + sizeof(offset_t) : (pc))

len_t count(Regex *re,
            len_t *aux_size,
            len_t *grp_cnt,
            len_t *counters_len,
            len_t *mem_len);
byte *emit(Regex *re, byte *pc, Program *prog);

Compiler *compiler(const Parser *p,
                   Construction  construction,
                   int           only_std_split,
                   SplitChoice   branch)
{
    Compiler *compiler       = malloc(sizeof(Compiler));
    compiler->parser         = p;
    compiler->construction   = construction;
    compiler->only_std_split = only_std_split;
    compiler->branch         = branch;
    return compiler;
}

void compiler_free(Compiler *compiler)
{
    parser_free((Parser *) compiler->parser);
    free(compiler);
}

Program *compile(const Compiler *compiler)
{
    len_t insts_size, aux_size = 0, grp_cnt = 0, counters_len = 0, mem_len = 0;
    Program *prog;
    Regex   *re;
    byte    *pc;

    re         = parse(compiler->parser);
    insts_size = count(re, &aux_size, &grp_cnt, &counters_len, &mem_len) + 1;
    prog       = program(insts_size, aux_size, grp_cnt, counters_len, mem_len);

    /* set the length fields to 0 as we use them for indices during emitting */
    prog->grp_cnt      = 0;
    prog->aux_len      = 0;
    prog->counters_len = 0;
    prog->mem_len      = 0;
    pc                 = emit(re, prog->insts, prog);
    *pc++              = MATCH;
    regex_free(re);

    return prog;
}

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

        case LITERAL: n = sizeof(byte) + sizeof(const char *); break;

        case CC:
            n          = sizeof(byte) + 2 * sizeof(len_t);
            *aux_size += re->cc_len * sizeof(Interval);
            break;

        case ALT:
            n  = 2 * sizeof(byte) + 3 * sizeof(offset_t);
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
            n  = 5 * sizeof(byte) + 5 * sizeof(offset_t) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            ++*mem_len;
            break;

        case PLUS:
            n  = 4 * sizeof(byte) + 3 * sizeof(offset_t) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            ++*mem_len;
            break;

        case QUES:
            n  = sizeof(byte) + 2 * sizeof(offset_t);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            break;

        case COUNTER:
            n = 11 * sizeof(byte) + 5 * sizeof(offset_t) + 6 * sizeof(len_t) +
                3 * sizeof(cntr_t);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            ++*counters_len;
            ++*mem_len;
            break;

        case LOOKAHEAD:
            n  = 2 * sizeof(byte) + 2 * sizeof(offset_t) + sizeof(int);
            n += count(re->left, aux_size, grp_cnt, counters_len, mem_len);
            break;
    }

    return n;
}

byte *emit(Regex *re, byte *pc, Program *prog)
{
    len_t     c, k;
    offset_t *p, *q, *r, *t;

    switch (re->type) {
        case CARET: *pc++ = BEGIN; break;
        case DOLLAR: *pc++ = END; break;

        /* `char` `ch` */
        case LITERAL:
            *pc++ = CHAR;
            MEMPUSH(pc, const char *, re->ch);
            break;

        /* `pred` l p */
        case CC:
            *pc++ = PRED;
            MEMPUSH(pc, len_t, re->cc_len);
            memcpy(prog->aux + prog->aux_len, re->intervals,
                   re->cc_len * sizeof(Interval));
            MEMPUSH(pc, len_t, prog->aux_len);
            prog->aux_len += re->cc_len * sizeof(Interval);
            break;

        /*     `split` L1, L2               *
         * L1: instructions for `re->left`  *
         *     `jmp` L3                     *
         * L2: instructions for `re->right` *
         * L3:                              */
        case ALT:
            *pc++  = SPLIT;
            p      = (offset_t *) pc;
            pc    += 2 * sizeof(offset_t);
            *p     = pc - (byte *) p;
            ++p;
            pc     = emit(re->left, pc, prog);
            *pc++  = JMP;
            q      = (offset_t *) pc;
            pc    += sizeof(offset_t);
            *p     = pc - (byte *) p;
            pc     = emit(re->right, pc, prog);
            *q     = pc - (byte *) q;
            break;

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
            *pc++ = SAVE;
            k     = prog->grp_cnt++;
            MEMPUSH(pc, len_t, 2 * k);
            pc    = emit(re->left, pc, prog);
            *pc++ = SAVE;
            MEMPUSH(pc, len_t, 2 * k + 1);
            break;

        /*     `split` L1, L3              -- L3, L1 if non-greedy *
         * L1: `epsset` k                                          *
         *     instructions for `re->left`                         *
         *     `split` L2, L3              -- L3, L2 if non-greedy *
         * L2: `epschk` k                                          *
         *     `jmp` L1                                            *
         * L3:                                                     */
        case STAR:
            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);

            *p              = pc - (byte *) p;
            p               = (offset_t *) pc;
            *pc++           = EPSSET;
            k               = prog->mem_len++;
            prog->memory[k] = 0;
            MEMPUSH(pc, len_t, k);
            pc = emit(re->left, pc, prog);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(r, t, re, pc);
            pc += 2 * sizeof(offset_t);

            *r    = pc - (byte *) r;
            *pc++ = EPSCHK;
            MEMPUSH(pc, len_t, k);
            *pc++ = JMP;
            MEMPUSH(pc, offset_t, (byte *) p - pc);

            *q = pc - (byte *) q;
            *t = pc - (byte *) t;
            break;

        /* L1: `epsset` k                                          *
         *     instructions for `re->left`                         *
         *     `split` L2, L3              -- L3, L2 if non-greedy *
         * L2: `epschk` k                                          *
         *     `jmp` L1                                            *
         * L3:                                                     */
        case PLUS:
            r               = (offset_t *) pc;
            *pc++           = EPSSET;
            k               = prog->mem_len++;
            prog->memory[k] = 0;
            MEMPUSH(pc, len_t, k);
            pc = emit(re->left, pc, prog);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);

            *p    = pc - (byte *) p;
            *pc++ = EPSCHK;
            MEMPUSH(pc, len_t, k);
            *pc++ = JMP;
            MEMPUSH(pc, offset_t, (byte *) r - pc);

            *q = pc - (byte *) q;
            break;

        /*     `split` L1, L2              -- L2, L1 if non-greedy *
         * L1: instructions for `re->left`                         *
         * L2:                                                     */
        case QUES:
            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);
            *p  = pc - (byte *) p;
            pc  = emit(re->left, pc, prog);
            *q  = pc - (byte *) q;
            break;

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
        case COUNTER:
            *pc++             = RESET;
            c                 = prog->counters_len++;
            prog->counters[c] = 0;
            MEMPUSH(pc, len_t, c);
            MEMPUSH(pc, cntr_t, 0);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);
            *p  = pc - (byte *) p;

            p     = (offset_t *) pc;
            *pc++ = CMP;
            MEMPUSH(pc, len_t, c);
            MEMPUSH(pc, cntr_t, re->max);
            *pc++ = LE;

            *pc++           = EPSSET;
            k               = prog->mem_len++;
            prog->memory[k] = 0;
            MEMPUSH(pc, len_t, k);
            pc    = emit(re->left, pc, prog);
            *pc++ = INC;
            MEMPUSH(pc, len_t, c);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(r, t, re, pc);
            pc += 2 * sizeof(offset_t);
            *r  = pc - (byte *) r;

            *pc++ = EPSCHK;
            MEMPUSH(pc, len_t, k);
            *pc++ = JMP;
            MEMPUSH(pc, offset_t, (byte *) p - pc);

            *q    = pc - (byte *) q;
            *t    = pc - (byte *) t;
            *pc++ = CMP;
            MEMPUSH(pc, len_t, c);
            MEMPUSH(pc, cntr_t, re->min);
            *pc++ = GE;
            break;

        /*     `zwa` L1, L2, `neg`         *
         * L1: instructions for `re->left` *
         *     `match`                     *
         * L2:                             */
        case LOOKAHEAD:
            *pc++  = ZWA;
            p      = (offset_t *) pc;
            pc    += 2 * sizeof(offset_t);
            *pc++  = re->pos;
            *p     = pc - (byte *) p;
            ++p;
            pc    = emit(re->left, pc, prog);
            *pc++ = MATCH;
            *p    = pc - (byte *) p;
            break;
    }

    return pc;
}
