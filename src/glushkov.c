#include <stdlib.h>
#include <string.h>

#include "glushkov.h"

#define SET_OFFSET(p, pc) (*(p) = (pc) - (byte *) ((p) + 1))

#define SPLIT_LABELS_PTRS(p, q, re, pc)                              \
    (p) = (offset_t *) ((re)->pos ? (pc) : (pc) + sizeof(offset_t)); \
    (q) = (offset_t *) ((re)->pos ? (pc) + sizeof(offset_t) : (pc))

typedef struct action Action;

struct action {
    size_t  pos;
    Action *next;
};

typedef struct pos_pair PosPair;

struct pos_pair {
    size_t   pos;     /*<< linearised position for Glushkov                   */
    Action  *actions; /*<< linked list of actions for transition              */
    PosPair *prev;
    PosPair *next;
};

typedef struct {
    PosPair *sentinal; /*<< sentinal for circly linked list                   */
    PosPair *gamma;    /*<< shortcut to gamma pair in linked list             */
} PosPairList;

typedef struct {
    PosPairList  *first;      /*<< first list for Glushkov                    */
    PosPairList  *last;       /*<< last *set* for Glushkov                    */
    PosPairList **follow;     /*<< integer map of positions to follow lists   */
    size_t        npositions; /*<< number of linearised positions             */
    const Regex **positions;  /*<< integer map of positions to actual info    */
} Rfa;

static PosPairList *pos_pair_list_new(void);
static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Regex **positions);

static size_t
count(const Regex *re, len_t *aux_len, len_t *ncaptures, len_t *ncounters);
static void  rfa_construct(const Regex *re, Rfa *rfa);
static void  rfa_absorb(Rfa *rfa);
static len_t rfa_insts_len(Rfa *rfa);
static byte *emit(const Regex *re, byte *pc, Program *prog);

const Program *glushkov_compile(const Regex *re)
{
    len_t  insts_len, npositions, aux_len = 0, ncaptures = 0, ncounters = 0;
    size_t i;
    PosPairList **follow;
    const Regex **positions;
    Rfa          *rfa;
    Program      *prog;
    byte         *pc;

    npositions = count(re, &aux_len, &ncaptures, &ncounters);
    positions  = malloc(npositions * sizeof(Regex *));
    follow     = malloc(npositions * sizeof(PosPairList *));
    for (i = 0; i < npositions; i++) follow[i] = pos_pair_list_new();

    rfa = rfa_new(follow, npositions, positions);
    rfa_construct(re, rfa);
    rfa_absorb(rfa);

    prog = program_new(rfa_insts_len(rfa), aux_len, ncaptures, ncounters, 0);

    /* set the length fields to 0 as we use them for indices during emitting */
    prog->aux_len = prog->ncounters = 0;
    pc                              = emit(re, prog->insts, prog);
    *pc                             = MATCH;

    return prog;
}

static PosPairList *pos_pair_list_new(void)
{
    PosPairList *list = malloc(sizeof(PosPairList));

    list->sentinal       = malloc(sizeof(PosPair));
    list->sentinal->prev = list->sentinal->next = list->sentinal;
    list->gamma                                 = NULL;

    return list;
}

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Regex **positions)
{
    Rfa *rfa = malloc(sizeof(Rfa));

    rfa->first      = pos_pair_list_new();
    rfa->last       = pos_pair_list_new();
    rfa->follow     = follow;
    rfa->npositions = npositions;
    rfa->positions  = positions;

    return rfa;
}

static size_t
count(const Regex *re, len_t *aux_len, len_t *ncaptures, len_t *ncounters)
{
    size_t npos = 0;

    switch (re->type) {
        case CARET:  /* fallthrough */
        case DOLLAR: /* fallthrough */
        case LITERAL: npos = 1; break;

        case CC:
            npos      = 1;
            *aux_len += re->cc_len * sizeof(Interval);
            break;

        case ALT: /* fallthrough */
        case CONCAT:
            npos = count(re->left, aux_len, ncaptures, ncounters) +
                   count(re->right, aux_len, ncaptures, ncounters);
            break;

        case CAPTURE:
            npos = 2 + count(re->left, aux_len, ncaptures, ncounters);
            if (*ncaptures < re->capture_idx + 1)
                *ncaptures = re->capture_idx + 1;
            break;

        case STAR: /* fallthrough */
        case PLUS: /* fallthrough */
        case QUES: npos = count(re->left, aux_len, ncaptures, ncounters); break;

        case COUNTER:
            npos = count(re->left, aux_len, ncaptures, ncounters);
            ++*ncounters;
            break;

        case LOOKAHEAD:
            npos = 1 + count(re->left, aux_len, ncaptures, ncounters);
            break;
    }

    return npos;
}

/* TODO: */
static void rfa_construct(const Regex *re, Rfa *rfa) {}

static void rfa_absorb(Rfa *rfa) {}

static len_t rfa_insts_len(Rfa *rfa)
{
    len_t insts_len = 0;

    return insts_len;
}

static byte *emit(const Regex *re, byte *pc, Program *prog)
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
            MEMWRITE(pc, const char *, re->ch);
            break;

        /* `pred` l p */
        case CC:
            *pc++ = PRED;
            MEMWRITE(pc, len_t, re->cc_len);
            memcpy(prog->aux + prog->aux_len, re->intervals,
                   re->cc_len * sizeof(Interval));
            MEMWRITE(pc, len_t, prog->aux_len);
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
            k     = re->capture_idx;
            MEMWRITE(pc, len_t, 2 * k);
            pc    = emit(re->left, pc, prog);
            *pc++ = SAVE;
            MEMWRITE(pc, len_t, 2 * k + 1);
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
            MEMWRITE(mem, const char *, NULL);
            MEMWRITE(pc, len_t, k);
            pc = emit(re->left, pc, prog);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(r, t, re, pc);
            pc += 2 * sizeof(offset_t);

            SET_OFFSET(r, pc);
            *pc++ = EPSCHK;
            MEMWRITE(pc, len_t, k);
            *pc++ = JMP;
            MEMWRITE(pc, offset_t, (byte *) p - (pc + sizeof(offset_t)));

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
            MEMWRITE(mem, const char *, NULL);
            MEMWRITE(pc, len_t, k);
            pc = emit(re->left, pc, prog);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);

            SET_OFFSET(p, pc);
            *pc++ = EPSCHK;
            MEMWRITE(pc, len_t, k);
            *pc++ = JMP;
            MEMWRITE(pc, offset_t, (byte *) r - (pc + sizeof(offset_t)));

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
            c                 = prog->ncounters++;
            prog->counters[c] = 0;
            MEMWRITE(pc, len_t, c);
            MEMWRITE(pc, cntr_t, 0);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(p, q, re, pc);
            pc += 2 * sizeof(offset_t);
            SET_OFFSET(p, pc);

            p     = (offset_t *) pc;
            *pc++ = CMP;
            MEMWRITE(pc, len_t, c);
            MEMWRITE(pc, cntr_t, re->max);
            *pc++ = LT;

            *pc++          = EPSSET;
            k              = prog->mem_len;
            prog->mem_len += sizeof(const char *);
            MEMWRITE(mem, const char *, NULL);
            MEMWRITE(pc, len_t, k);
            pc    = emit(re->left, pc, prog);
            *pc++ = INC;
            MEMWRITE(pc, len_t, c);

            *pc++ = SPLIT;
            SPLIT_LABELS_PTRS(r, t, re, pc);
            pc += 2 * sizeof(offset_t);
            SET_OFFSET(r, pc);

            *pc++ = EPSCHK;
            MEMWRITE(pc, len_t, k);
            *pc++ = JMP;
            MEMWRITE(pc, offset_t, (byte *) p - (pc + sizeof(offset_t)));

            SET_OFFSET(q, pc);
            SET_OFFSET(t, pc);
            *pc++ = CMP;
            MEMWRITE(pc, len_t, c);
            MEMWRITE(pc, cntr_t, re->min);
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
