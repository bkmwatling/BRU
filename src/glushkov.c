#include <stdlib.h>
#include <string.h>

#include "glushkov.h"

#define SET_OFFSET(p, pc) (*(p) = (pc) - (byte *) ((p) + 1))

#define SPLIT_LABELS_PTRS(p, q, re, pc)                              \
    (p) = (offset_t *) ((re)->pos ? (pc) : (pc) + sizeof(offset_t)); \
    (q) = (offset_t *) ((re)->pos ? (pc) + sizeof(offset_t) : (pc))

#define IS_EMPTY(pos_pair_list) \
    ((pos_pair_list)->sentinal->next == (pos_pair_list)->sentinal)

#define FOREACH(pos_pair, pos_pair_list)               \
    for ((pos_pair) = (pos_pair_list)->sentinal->next; \
         (pos_pair) != (pos_pair_list)->sentinal;      \
         (pos_pair) = (pos_pair)->next)

#define GAMMA_POS 0

#define NULLABLE(rfa) ((rfa)->first->gamma != NULL)

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

static PosPair *pos_pair_insert_after(size_t pos, PosPair *target);

static PosPairList *pos_pair_list_new(void);
static void         pos_pair_list_free(PosPairList *list);
static PosPairList *pos_pair_list_clone(PosPairList *list);
static void         pos_pair_list_clear(PosPairList *list);
static void         pos_pair_list_remove_gamma(PosPairList *list);
static void pos_pair_list_replace_gamma(PosPairList *list1, PosPairList *list2);
static void pos_pair_list_append(PosPairList *list1, PosPairList *list2);

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Regex **positions);
static void rfa_free(Rfa *rfa);

static size_t
count(const Regex *re, len_t *aux_len, len_t *ncaptures, len_t *ncounters);
static void  rfa_construct(const Regex *re, Rfa *rfa);
static void  rfa_absorb(Rfa *rfa);
static len_t rfa_insts_len(Rfa *rfa);
static byte *emit(const Rfa *rfa, byte *pc, Program *prog);

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

    rfa = rfa_new(follow, 1, positions);
    rfa_construct(re, rfa);
    rfa_absorb(rfa);

    prog = program_new(rfa_insts_len(rfa), aux_len, ncaptures, ncounters, 0);

    /* set the length fields to 0 as we use them for indices during emitting */
    prog->aux_len = prog->ncounters = 0;
    pc                              = emit(rfa, prog->insts, prog);
    *pc                             = MATCH;

    rfa_free(rfa);
    for (i = 0; i < npositions; i++) {
        pos_pair_list_free(follow[i]);
        if (positions[i]->type == CAPTURE) free((Regex *) positions[i]);
    }
    free(follow);
    free(positions);

    return prog;
}

static PosPair *pos_pair_insert_after(size_t pos, PosPair *target)
{
    PosPair *pos_pair = malloc(sizeof(PosPair));

    pos_pair->pos      = pos;
    pos_pair->prev     = target;
    pos_pair->next     = target->next;
    target->next       = pos_pair;
    target->next->prev = pos_pair;

    return pos_pair;
}

static PosPairList *pos_pair_list_new(void)
{
    PosPairList *list = malloc(sizeof(PosPairList));

    list->sentinal       = malloc(sizeof(PosPair));
    list->sentinal->prev = list->sentinal->next = list->sentinal;
    list->gamma                                 = NULL;

    return list;
}

static void pos_pair_list_free(PosPairList *list)
{
    pos_pair_list_clear(list);
    free(list->sentinal);
    free(list);
}

static PosPairList *pos_pair_list_clone(PosPairList *list)
{
    PosPairList *ppl = pos_pair_list_new();
    PosPair     *pp;

    FOREACH(pp, list) pos_pair_insert_after(pp->pos, ppl->sentinal->prev);

    return ppl;
}

static void pos_pair_list_clear(PosPairList *list)
{
    PosPair *pp;

    while (list->sentinal->next != list->sentinal) {
        pp                   = list->sentinal->next;
        list->sentinal->next = pp->next;
        pp->prev = pp->next = NULL;
        free(pp);
    }

    list->sentinal->prev = list->sentinal->next = NULL;
    list->gamma                                 = NULL;
}

static void pos_pair_list_remove_gamma(PosPairList *list)
{
    if (list->gamma == NULL) return;

    list->gamma->prev->next = list->gamma->next;
    list->gamma->next->prev = list->gamma->prev;
    list->gamma->prev = list->gamma->next = NULL;

    free(list->gamma);
    list->gamma = NULL;
}

static void pos_pair_list_replace_gamma(PosPairList *list1, PosPairList *list2)
{
    if (list1->gamma == NULL) return;
    if (IS_EMPTY(list2)) {
        pos_pair_list_remove_gamma(list1);
        return;
    }

    list1->gamma->prev->next    = list2->sentinal->next;
    list2->sentinal->next->prev = list1->gamma->prev;
    list2->sentinal->prev->next = list1->gamma->next;
    list1->gamma->next->prev    = list2->sentinal->prev;
    list1->gamma->prev = list1->gamma->next = NULL;
    list2->sentinal->prev = list2->sentinal->next = list2->sentinal;

    list1->gamma->prev = list1->gamma->next = NULL;
    free(list1->gamma);
    list1->gamma = list2->gamma;
    list2->gamma = NULL;
}

static void pos_pair_list_append(PosPairList *list1, PosPairList *list2)
{
    if (IS_EMPTY(list2)) return;

    list1->sentinal->prev->next = list2->sentinal->next;
    list2->sentinal->next->prev = list1->sentinal->prev;
    list2->sentinal->prev->next = list1->sentinal;
    list1->sentinal->prev       = list2->sentinal->prev;
    list2->sentinal->prev = list2->sentinal->next = list2->sentinal;

    if (list1->gamma == NULL) list1->gamma = list2->gamma;
    list2->gamma = NULL;
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

static void rfa_free(Rfa *rfa)
{
    pos_pair_list_free(rfa->first);
    pos_pair_list_free(rfa->last);
    free(rfa);
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

    return npos + 1; /* + 1 so that 0 is gamma */
}

static void rfa_construct(const Regex *re, Rfa *rfa)
{
    Regex       *re_tmp;
    Rfa         *rfa_tmp;
    PosPairList *ppl_tmp;
    PosPair     *pp;
    size_t       pos;
    size_t       pos_open, pos_close; /* for capture positions */

    switch (re->type) {
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case LITERAL: /* fallthrough */
        case CC:
            pos                 = rfa->npositions++;
            rfa->positions[pos] = re;
            pos_pair_insert_after(GAMMA_POS, rfa->follow[pos]->sentinal);
            pos_pair_insert_after(pos, rfa->first->sentinal);
            pos_pair_insert_after(pos, rfa->last->sentinal);
            break;

        case ALT:
            rfa_construct(re->left, rfa);
            rfa_tmp = rfa_new(rfa->follow, rfa->npositions, rfa->positions);
            rfa->npositions += rfa_tmp->npositions;

            if (NULLABLE(rfa)) pos_pair_list_remove_gamma(rfa_tmp->first);
            pos_pair_list_append(rfa->first, rfa_tmp->first);
            pos_pair_list_append(rfa->last, rfa_tmp->last);

            rfa_free(rfa_tmp);
            break;

        case CONCAT:
            rfa_construct(re->left, rfa);
            rfa_tmp = rfa_new(rfa->follow, rfa->npositions, rfa->positions);
            rfa->npositions += rfa_tmp->npositions;

            FOREACH(pp, rfa->last) {
                ppl_tmp = pos_pair_list_clone(rfa_tmp->first);
                pos_pair_list_replace_gamma(rfa->follow[pp->pos], ppl_tmp);
                pos_pair_list_free(ppl_tmp);
            }

            if (NULLABLE(rfa))
                pos_pair_list_replace_gamma(rfa->first, rfa_tmp->first);

            ppl_tmp       = rfa->last;
            rfa->last     = rfa_tmp->last;
            rfa_tmp->last = ppl_tmp;
            if (NULLABLE(rfa_tmp))
                pos_pair_list_append(rfa->last, rfa_tmp->last);

            rfa_free(rfa_tmp);
            break;

        case CAPTURE:
            pos_open = rfa->npositions++;
            re_tmp   = malloc(sizeof(Regex));
            memcpy(re_tmp, re, sizeof(Regex));
            re_tmp->capture_idx      *= 2;
            rfa->positions[pos_open]  = re_tmp;
            rfa_construct(re->left, rfa);
            pos_close = rfa->npositions++;
            re_tmp    = malloc(sizeof(Regex));
            memcpy(re_tmp, re, sizeof(Regex));
            re_tmp->capture_idx       = re_tmp->capture_idx * 2 + 1;
            rfa->positions[pos_close] = re_tmp;

            if (NULLABLE(rfa))
                pos_pair_insert_after(pos_open, rfa->last->sentinal);
            FOREACH(pp, rfa->last) {
                pos_pair_insert_after(pos_close, rfa->follow[pp->pos]->gamma);
                pos_pair_list_remove_gamma(rfa->follow[pp->pos]);
            }
            pos_pair_list_append(rfa->follow[pos_open], rfa->first);
            pos_pair_insert_after(GAMMA_POS, rfa->follow[pos_close]->sentinal);
            pos_pair_list_clear(rfa->first);
            pos_pair_list_clear(rfa->last);

            pos_pair_insert_after(pos_open, rfa->first->sentinal);
            pos_pair_insert_after(pos_close, rfa->last->sentinal);

            break;

        case STAR:
            rfa_construct(re->left, rfa);
            if (re->pos) {
                if (NULLABLE(rfa))
                    pos_pair_insert_after(GAMMA_POS,
                                          rfa->first->sentinal->prev);
            } else {
                pos_pair_list_remove_gamma(rfa->first);
                pos_pair_insert_after(GAMMA_POS, rfa->first->sentinal);
            }

            FOREACH(pp, rfa->last) {
                ppl_tmp = pos_pair_list_clone(rfa->first);
                pos_pair_list_replace_gamma(rfa->follow[pp->pos], ppl_tmp);
                pos_pair_list_free(ppl_tmp);
            }
            break;

        case PLUS:
            rfa_construct(re->left, rfa);
            FOREACH(pp, rfa->last) {
                ppl_tmp = pos_pair_list_clone(rfa->first);
                if (re->pos) {
                    if (NULLABLE(rfa))
                        pos_pair_insert_after(GAMMA_POS,
                                              ppl_tmp->sentinal->prev);
                } else {
                    pos_pair_list_remove_gamma(ppl_tmp);
                    pos_pair_insert_after(GAMMA_POS, ppl_tmp->sentinal);
                }
                pos_pair_list_replace_gamma(rfa->follow[pp->pos], ppl_tmp);
                pos_pair_list_free(ppl_tmp);
            }
            break;

        case QUES:
            rfa_construct(re->left, rfa);
            if (re->pos) {
                if (NULLABLE(rfa))
                    pos_pair_insert_after(GAMMA_POS,
                                          rfa->first->sentinal->prev);
            } else {
                pos_pair_list_remove_gamma(rfa->first);
                pos_pair_insert_after(GAMMA_POS, rfa->first->sentinal);
            }
            break;

        /* TODO: */
        case COUNTER: break;
        case LOOKAHEAD: break;
    }
}

/* TODO: */
static void rfa_absorb(Rfa *rfa) {}

static len_t rfa_insts_len(Rfa *rfa)
{
    len_t insts_len = 0;

    return insts_len;
}

static byte *emit(const Rfa *rfa, byte *pc, Program *prog) { return pc; }
