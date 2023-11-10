#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "sre.h"

#define COMPILER_OPTS_DEFAULT ((CompilerOpts){ THOMPSON, FALSE, SC_SPLIT })

#define SET_OFFSET(p, pc) (*(p) = pc - (byte *) ((p) + 1))

#define SPLIT_LABELS_PTRS(p, q, re, pc)                              \
    (p) = (offset_t *) ((re)->pos ? (pc) : (pc) + sizeof(offset_t)); \
    (q) = (offset_t *) ((re)->pos ? (pc) + sizeof(offset_t) : (pc))

static len_t count(Regex *re,
                   len_t *aux_size,
                   len_t *ncaptures,
                   len_t *counters_len,
                   len_t *mem_len);
static byte *emit(Regex *re, byte *pc, Program *prog);

Compiler *compiler_new(const Parser *parser, CompilerOpts *opts)
{
    Compiler *compiler = malloc(sizeof(Compiler));
    compiler->parser   = parser;
    compiler->opts     = opts ? *opts : COMPILER_OPTS_DEFAULT;
    return compiler;
}

void compiler_free(Compiler *self)
{
    parser_free((Parser *) self->parser);
    free(self);
}

Program *compiler_compile(const Compiler *self)
{
    len_t insts_size, aux_size = 0, ncaptures = 0, counters_len = 0,
                      mem_len = 0;
    Program *prog;
    Regex   *re;
    byte    *pc;

    re         = parser_parse(self->parser);
    insts_size = count(re, &aux_size, &ncaptures, &counters_len, &mem_len) + 1;
    prog = program_new(insts_size, aux_size, ncaptures, counters_len, mem_len);

    /* set the length fields to 0 as we use them for indices during emitting */
    prog->ncaptures    = 0;
    prog->aux_len      = 0;
    prog->counters_len = 0;
    prog->mem_len      = 0;
    pc                 = emit(re, prog->insts, prog);
    *pc++              = MATCH;
    regex_free(re);

    return prog;
}

static len_t count(Regex *re,
                   len_t *aux_size,
                   len_t *ncaptures,
                   len_t *counters_len,
                   len_t *mem_len)
{
    len_t n = 0;

    switch (re->type) {
        case CARET: /* fallthrough */
        case DOLLAR: n = sizeof(byte); break;

        case LITERAL: n = sizeof(byte) + sizeof(const char *); break;

        case CC:
            n          = sizeof(byte) + 2 * sizeof(len_t);
            *aux_size += re->cc_len * sizeof(Interval);
            break;

        case ALT:
            n  = 2 * sizeof(byte) + 3 * sizeof(offset_t);
            n += count(re->left, aux_size, ncaptures, counters_len, mem_len) +
                 count(re->right, aux_size, ncaptures, counters_len, mem_len);
            break;

        case CONCAT:
            n = count(re->left, aux_size, ncaptures, counters_len, mem_len) +
                count(re->right, aux_size, ncaptures, counters_len, mem_len);
            break;

        case CAPTURE:
            n  = 2 * sizeof(byte) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, ncaptures, counters_len, mem_len);
            ++*ncaptures;
            break;

        case STAR:
            n  = 5 * sizeof(byte) + 5 * sizeof(offset_t) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, ncaptures, counters_len, mem_len);
            *mem_len += sizeof(char *);
            break;

        case PLUS:
            n  = 4 * sizeof(byte) + 3 * sizeof(offset_t) + 2 * sizeof(len_t);
            n += count(re->left, aux_size, ncaptures, counters_len, mem_len);
            *mem_len += sizeof(char *);
            break;

        case QUES:
            n  = sizeof(byte) + 2 * sizeof(offset_t);
            n += count(re->left, aux_size, ncaptures, counters_len, mem_len);
            break;

        case COUNTER:
            n = 11 * sizeof(byte) + 5 * sizeof(offset_t) + 6 * sizeof(len_t) +
                3 * sizeof(cntr_t);
            n += count(re->left, aux_size, ncaptures, counters_len, mem_len);
            ++*counters_len;
            *mem_len += sizeof(char *);
            break;

        case LOOKAHEAD:
            n  = 3 * sizeof(byte) + 2 * sizeof(offset_t);
            n += count(re->left, aux_size, ncaptures, counters_len, mem_len);
            break;
    }

    return n;
}

static byte *emit(Regex *re, byte *pc, Program *prog)
{
    len_t     c, k;
    offset_t *p, *q, *r, *t;
    byte     *mem = prog->memory + prog->mem_len;

    switch (re->type) {
        case CARET: *pc++ = BEGIN; break;
        case DOLLAR: *pc++ = END; break;

        /* `char` ch */
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
            SET_OFFSET(p, pc);
            ++p;
            pc     = emit(re->left, pc, prog);
            *pc++  = JMP;
            q      = (offset_t *) pc;
            pc    += sizeof(offset_t);
            SET_OFFSET(p, pc);
            pc = emit(re->right, pc, prog);
            SET_OFFSET(q, pc);
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
            k     = prog->ncaptures++;
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

            SET_OFFSET(p, pc);
            p              = (offset_t *) pc;
            *pc++          = EPSSET;
            k              = prog->mem_len;
            prog->mem_len += sizeof(const char *);
            MEMPUSH(mem, const char *, NULL);
            MEMPUSH(pc, len_t, k);
            pc = emit(re->left, pc, prog);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(r, t, re, pc);
            pc += 2 * sizeof(offset_t);

            SET_OFFSET(r, pc);
            *pc++ = EPSCHK;
            MEMPUSH(pc, len_t, k);
            *pc++ = JMP;
            MEMPUSH(pc, offset_t, (byte *) p - (pc + sizeof(offset_t)));

            SET_OFFSET(q, pc);
            SET_OFFSET(t, pc);
            break;

        /* L1: `epsset` k                                          *
         *     instructions for `re->left`                         *
         *     `split` L2, L3              -- L3, L2 if non-greedy *
         * L2: `epschk` k                                          *
         *     `jmp` L1                                            *
         * L3:                                                     */
        case PLUS:
            r              = (offset_t *) pc;
            *pc++          = EPSSET;
            k              = prog->mem_len;
            prog->mem_len += sizeof(const char *);
            MEMPUSH(mem, const char *, NULL);
            MEMPUSH(pc, len_t, k);
            pc = emit(re->left, pc, prog);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);

            SET_OFFSET(p, pc);
            *pc++ = EPSCHK;
            MEMPUSH(pc, len_t, k);
            *pc++ = JMP;
            MEMPUSH(pc, offset_t, (byte *) r - (pc + sizeof(offset_t)));

            SET_OFFSET(q, pc);
            break;

        /*     `split` L1, L2              -- L2, L1 if non-greedy *
         * L1: instructions for `re->left`                         *
         * L2:                                                     */
        case QUES:
            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);
            SET_OFFSET(p, pc);
            pc = emit(re->left, pc, prog);
            SET_OFFSET(q, pc);
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
            SET_OFFSET(p, pc);

            p     = (offset_t *) pc;
            *pc++ = CMP;
            MEMPUSH(pc, len_t, c);
            MEMPUSH(pc, cntr_t, re->max);
            *pc++ = LE;

            *pc++          = EPSSET;
            k              = prog->mem_len;
            prog->mem_len += sizeof(const char *);
            MEMPUSH(mem, const char *, NULL);
            MEMPUSH(pc, len_t, k);
            pc    = emit(re->left, pc, prog);
            *pc++ = INC;
            MEMPUSH(pc, len_t, c);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(r, t, re, pc);
            pc += 2 * sizeof(offset_t);
            SET_OFFSET(r, pc);

            *pc++ = EPSCHK;
            MEMPUSH(pc, len_t, k);
            *pc++ = JMP;
            MEMPUSH(pc, offset_t, (byte *) p - (pc + sizeof(offset_t)));

            SET_OFFSET(q, pc);
            SET_OFFSET(t, pc);
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
            SET_OFFSET(p, pc);
            ++p;
            pc    = emit(re->left, pc, prog);
            *pc++ = MATCH;
            SET_OFFSET(p, pc);
            break;
    }

    return pc;
}
