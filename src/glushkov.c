#include <assert.h>
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

#define IS_NONEPS_TRANSITION(re) \
    ((re) == NULL || (re)->type == LITERAL || (re)->type == CC)

/* START_POS and GAMMA_POS should be the same since gamma isn't an actual
 * position and the start position/state should not follow from others        */
#define START_POS 0
#define GAMMA_POS 0

#define FIRST(rfa)      ((rfa)->follow[START_POS])
#define NULLABLE(first) (first->gamma != NULL)

#define RFA_NEW_FROM(rfa) \
    rfa_new((rfa)->follow, (rfa)->npositions, (rfa)->positions)

typedef struct action Action;

struct action {
    size_t  pos;
    Action *next;
};

typedef struct pos_pair PosPair;

struct pos_pair {
    size_t   pos;          /*<< linearised position for Glushkov              */
    len_t    nactions;     /*<< number of actions for transition              */
    Action  *first_action; /*<< first action in linked list of actions        */
    Action  *last_action;  /*<< last action in linked list of actions         */
    PosPair *prev;
    PosPair *next;
};

typedef struct {
    size_t   len;      /*<< length of (number of elements in) the list        */
    PosPair *sentinal; /*<< sentinal for circly linked list                   */
    PosPair *gamma;    /*<< shortcut to gamma pair in linked list             */
} PosPairList;

typedef struct {
    PosPairList  *last;       /*<< last *set* for Glushkov                    */
    PosPairList **follow;     /*<< integer map of positions to follow lists   */
    size_t        npositions; /*<< number of linearised positions             */
    const Regex **positions;  /*<< integer map of positions to actual info    */
} Rfa;

static Action *action_new(size_t pos, const Regex **positions);
static void    action_free(Action *action);
static Action *action_clone(const Action *action);

static void     pos_pair_free(PosPair *pos_pair);
static PosPair *pos_pair_clone(const PosPair *pos_pair);
static void     pos_pair_action_prepend(PosPair *pos_pair, Action *action);
static void     pos_pair_action_append(PosPair *pos_pair, Action *action);
static void     pos_pair_insert_after(PosPair *pos_pair, PosPair *target);
static PosPair *pos_pair_insert_pos_after(size_t pos, PosPair *target);
static PosPair *pos_pair_remove(PosPair *pos_pair);
static int      pos_pair_remove_from(size_t pos, PosPair *start, PosPair *end);

static PosPairList *pos_pair_list_new(void);
static void         pos_pair_list_free(PosPairList *list);
static PosPairList *pos_pair_list_clone(const PosPairList *list);
static void         pos_pair_list_clear(PosPairList *list);
static void         pos_pair_list_remove(PosPairList *list, size_t pos);
static void pos_pair_list_replace_gamma(PosPairList *list1, PosPairList *list2);
static void pos_pair_list_append(PosPairList *list1, PosPairList *list2);

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Regex **positions);
static void rfa_free(Rfa *rfa);

static size_t
count(const Regex *re, len_t *aux_len, len_t *ncaptures, len_t *ncounters);
static void  rfa_construct(const Regex *re, Rfa *rfa, PosPairList *first);
static void  rfa_absorb(Rfa *rfa, size_t pos, int *visited);
static len_t rfa_insts_len(Rfa *rfa);
static byte *emit(const Rfa *rfa, byte *pc, Program *prog);

const Program *glushkov_compile(const Regex *re)
{
    len_t  insts_len, npositions, aux_len = 0, ncaptures = 0, ncounters = 0;
    size_t i;
    PosPairList **follow;
    const Regex **positions;
    int          *visited;
    Rfa          *rfa;
    Program      *prog;
    byte         *pc;

    npositions = count(re, &aux_len, &ncaptures, &ncounters);
    positions  = malloc(npositions * sizeof(Regex *));
    follow     = malloc(npositions * sizeof(PosPairList *));
    visited    = malloc(npositions * sizeof(int));
    memset(visited, 0, npositions * sizeof(int));
    for (i = 0; i < npositions; i++) follow[i] = pos_pair_list_new();

    positions[START_POS] = NULL;
    rfa                  = rfa_new(follow, 1, positions);
    rfa_construct(re, rfa, NULL);
    rfa_absorb(rfa, START_POS, visited);

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

static Action *action_new(size_t pos, const Regex **positions)
{
    Action      *action = malloc(sizeof(Action));
    const Regex *re     = positions[pos];

    action->next = NULL;
    switch (re->type) {
        case CAPTURE: action->pos = re->capture_idx; break;
        default: assert(0 && "unreachable");
    }

    return action;
}

static void action_free(Action *action)
{
    action->next = NULL;
    free(action);
}

static Action *action_clone(const Action *action)
{
    Action *act = malloc(sizeof(Action));

    act->pos  = action->pos;
    act->next = NULL;

    return act;
}

static void pos_pair_free(PosPair *pos_pair)
{
    Action *action, *next;

    for (action = pos_pair->first_action; action; action = next) {
        next = action->next;
        action_free(action);
    }

    pos_pair->first_action = pos_pair->last_action = NULL;
    pos_pair->prev = pos_pair->next = NULL;
    free(pos_pair);
}

static PosPair *pos_pair_clone(const PosPair *pos_pair)
{
    Action  *action;
    PosPair *pp = malloc(sizeof(PosPair));

    pp->pos      = pos_pair->pos;
    pp->nactions = pos_pair->nactions;
    pp->prev = pp->next = NULL;
    for (action = pos_pair->first_action; action; action = action->next)
        pos_pair_action_append(pp, action_clone(action));

    return pp;
}

static void pos_pair_action_prepend(PosPair *pos_pair, Action *action)
{
    if (pos_pair->first_action) {
        action->next           = pos_pair->first_action;
        pos_pair->first_action = action;
    } else {
        pos_pair->first_action = pos_pair->last_action = action;
        action->next                                   = NULL;
    }
    pos_pair->nactions++;
}

static void pos_pair_action_append(PosPair *pos_pair, Action *action)
{
    if (pos_pair->last_action) {
        pos_pair->last_action->next = action;
    } else {
        pos_pair->first_action = pos_pair->last_action = action;
    }
    pos_pair->nactions++;
}

static void pos_pair_insert_after(PosPair *pos_pair, PosPair *target)
{
    pos_pair->prev       = target;
    pos_pair->next       = target->next;
    target->next         = pos_pair;
    pos_pair->next->prev = pos_pair;
}

static PosPair *pos_pair_insert_pos_after(size_t pos, PosPair *target)
{
    PosPair *pos_pair = malloc(sizeof(PosPair));

    pos_pair->pos          = pos;
    pos_pair->nactions     = 0;
    pos_pair->first_action = pos_pair->last_action = NULL;
    pos_pair_insert_after(pos_pair, target);

    return pos_pair;
}

static PosPair *pos_pair_remove(PosPair *pos_pair)
{
    PosPair *pp = pos_pair->next;

    if (pos_pair->prev) pos_pair->prev->next = pos_pair->next;
    if (pos_pair->next) pos_pair->next->prev = pos_pair->prev;
    pos_pair->prev = pos_pair->next = NULL;

    pos_pair_free(pos_pair);
    return pp;
}

static int pos_pair_remove_from(size_t pos, PosPair *start, PosPair *end)
{
    PosPair *pp;

    for (pp = start; pp && pp != end; pp = pp->next) {
        if (pp->pos == pos) {
            pos_pair_remove(pp);
            return TRUE;
        }
    }

    return FALSE;
}

static PosPairList *pos_pair_list_new(void)
{
    PosPairList *list = malloc(sizeof(PosPairList));

    list->len            = 0;
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

static PosPairList *pos_pair_list_clone(const PosPairList *list)
{
    PosPairList *ppl = pos_pair_list_new();
    PosPair     *pp;

    ppl->len = list->len;
    FOREACH(pp, list)
        pos_pair_insert_after(pos_pair_clone(pp), ppl->sentinal->prev);

    return ppl;
}

static void pos_pair_list_clear(PosPairList *list)
{
    PosPair *pp;

    while (list->sentinal->next != list->sentinal) {
        pp                   = list->sentinal->next;
        list->sentinal->next = pp->next;
        pp->prev = pp->next = NULL;
        pos_pair_free(pp);
    }

    list->sentinal->prev = list->sentinal->next = NULL;
    list->len                                   = 0;
    list->gamma                                 = NULL;
}

static void pos_pair_list_remove(PosPairList *list, size_t pos)
{
    PosPair *pp;

    if (pos == GAMMA_POS) {
        pp = list->gamma;
    } else {
        FOREACH(pp, list) {
            if (pp->pos == pos) break;
        }
    }

    if (pp == NULL) return;

    pos_pair_remove(pp);
    list->len--;
    if (pos == GAMMA_POS) list->gamma = NULL;
}

static void pos_pair_list_replace_gamma(PosPairList *list1, PosPairList *list2)
{
    if (list1->gamma == NULL) return;
    if (IS_EMPTY(list2)) {
        pos_pair_list_remove(list1, GAMMA_POS);
        return;
    }

    list1->gamma->prev->next    = list2->sentinal->next;
    list2->sentinal->next->prev = list1->gamma->prev;
    list2->sentinal->prev->next = list1->gamma->next;
    list1->gamma->next->prev    = list2->sentinal->prev;
    list1->gamma->prev = list1->gamma->next  = NULL;
    list1->len                              += list2->len - 1;
    list2->sentinal->prev = list2->sentinal->next = list2->sentinal;
    list2->len                                    = 0;

    free(list1->gamma);
    list1->gamma = list2->gamma;
    list2->gamma = NULL;
}

static void pos_pair_list_append(PosPairList *list1, PosPairList *list2)
{
    if (IS_EMPTY(list2)) return;

    list1->sentinal->prev->next  = list2->sentinal->next;
    list2->sentinal->next->prev  = list1->sentinal->prev;
    list2->sentinal->prev->next  = list1->sentinal;
    list1->sentinal->prev        = list2->sentinal->prev;
    list1->len                  += list2->len;
    list2->sentinal->prev = list2->sentinal->next = list2->sentinal;
    list2->len                                    = 0;

    if (list1->gamma == NULL) list1->gamma = list2->gamma;
    list2->gamma = NULL;
}

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Regex **positions)
{
    Rfa *rfa = malloc(sizeof(Rfa));

    rfa->last       = pos_pair_list_new();
    rfa->follow     = follow;
    rfa->npositions = npositions;
    rfa->positions  = positions;

    return rfa;
}

static void rfa_free(Rfa *rfa)
{
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

    return npos + 1; /* + 1 so that 0 is gamma/start */
}

static void rfa_construct(const Regex *re, Rfa *rfa, PosPairList *first)
{
    Regex       *re_tmp;
    Rfa         *rfa_tmp;
    PosPairList *ppl_tmp, *first_tmp;
    PosPair     *pp;
    size_t       pos;
    size_t       pos_open, pos_close; /* for capture positions */

    if (first == NULL) first = FIRST(rfa);
    switch (re->type) {
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case LITERAL: /* fallthrough */
        case CC:
            pos                 = rfa->npositions++;
            rfa->positions[pos] = re;
            pos_pair_insert_pos_after(GAMMA_POS, rfa->follow[pos]->sentinal);
            rfa->follow[pos]->len++;
            pos_pair_insert_pos_after(pos, first->sentinal);
            first->len++;
            pos_pair_insert_pos_after(pos, rfa->last->sentinal);
            rfa->last->len++;
            break;

        case ALT:
            rfa_construct(re->left, rfa, NULL);
            rfa_tmp   = RFA_NEW_FROM(rfa);
            first_tmp = pos_pair_list_new();
            rfa_construct(re->right, rfa_tmp, first_tmp);
            rfa->npositions += rfa_tmp->npositions;

            if (NULLABLE(first)) pos_pair_list_remove(first_tmp, GAMMA_POS);
            pos_pair_list_append(first, first_tmp);
            pos_pair_list_append(rfa->last, rfa_tmp->last);

            pos_pair_list_free(first_tmp);
            rfa_free(rfa_tmp);
            break;

        case CONCAT:
            rfa_construct(re->left, rfa, first);
            rfa_tmp   = RFA_NEW_FROM(rfa);
            first_tmp = pos_pair_list_new();
            rfa_construct(re->right, rfa_tmp, first_tmp);
            rfa->npositions += rfa_tmp->npositions;

            FOREACH(pp, rfa->last) {
                ppl_tmp = pos_pair_list_clone(first_tmp);
                pos_pair_list_replace_gamma(rfa->follow[pp->pos], ppl_tmp);
                pos_pair_list_free(ppl_tmp);
            }

            if (NULLABLE(first)) pos_pair_list_replace_gamma(first, first_tmp);

            ppl_tmp       = rfa->last;
            rfa->last     = rfa_tmp->last;
            rfa_tmp->last = ppl_tmp;
            if (NULLABLE(first_tmp))
                pos_pair_list_append(rfa->last, rfa_tmp->last);

            pos_pair_list_free(first_tmp);
            rfa_free(rfa_tmp);
            break;

        case CAPTURE:
            pos_open = rfa->npositions++;
            /* make copy of re to set capture_idx using strategy for VM */
            re_tmp   = malloc(sizeof(Regex));
            memcpy(re_tmp, re, sizeof(Regex));
            re_tmp->capture_idx      *= 2;
            rfa->positions[pos_open]  = re_tmp;

            rfa_construct(re->left, rfa, first);

            pos_close = rfa->npositions++;
            /* make copy of re to set capture_idx using strategy for VM */
            re_tmp    = malloc(sizeof(Regex));
            memcpy(re_tmp, re, sizeof(Regex));
            re_tmp->capture_idx       = re_tmp->capture_idx * 2 + 1;
            rfa->positions[pos_close] = re_tmp;

            /* allows next loop to handle gamma replace */
            if (NULLABLE(first)) {
                pos_pair_insert_pos_after(pos_open, rfa->last->sentinal);
                rfa->last->len++;
            }
            FOREACH(pp, rfa->last) {
                pos_pair_insert_pos_after(pos_close,
                                          rfa->follow[pp->pos]->gamma);
                rfa->follow[pp->pos]->len++;
                pos_pair_list_remove(rfa->follow[pp->pos], GAMMA_POS);
            }
            pos_pair_list_append(rfa->follow[pos_open], first);
            pos_pair_insert_pos_after(GAMMA_POS,
                                      rfa->follow[pos_close]->sentinal);
            rfa->follow[pos_close]->len++;
            pos_pair_list_clear(rfa->last);

            pos_pair_insert_pos_after(pos_open, first->sentinal);
            first->len++;
            pos_pair_insert_pos_after(pos_close, rfa->last->sentinal);
            rfa->last->len++;

            break;

        case STAR:
            rfa_construct(re->left, rfa, first);
            if (re->pos) {
                if (!NULLABLE(first)) {
                    pos_pair_insert_pos_after(GAMMA_POS, first->sentinal->prev);
                    first->len++;
                }
            } else {
                pos_pair_list_remove(first, GAMMA_POS);
                pos_pair_insert_pos_after(GAMMA_POS, first->sentinal);
                first->len++;
            }

            FOREACH(pp, rfa->last) {
                ppl_tmp = pos_pair_list_clone(first);
                pos_pair_list_replace_gamma(rfa->follow[pp->pos], ppl_tmp);
                pos_pair_list_free(ppl_tmp);
            }
            break;

        case PLUS:
            rfa_construct(re->left, rfa, first);
            FOREACH(pp, rfa->last) {
                ppl_tmp = pos_pair_list_clone(first);
                if (re->pos) {
                    if (!NULLABLE(first)) {
                        pos_pair_insert_pos_after(GAMMA_POS,
                                                  ppl_tmp->sentinal->prev);
                        ppl_tmp->len++;
                    }
                } else {
                    pos_pair_list_remove(ppl_tmp, GAMMA_POS);
                    pos_pair_insert_pos_after(GAMMA_POS, ppl_tmp->sentinal);
                    ppl_tmp->len++;
                }
                pos_pair_list_replace_gamma(rfa->follow[pp->pos], ppl_tmp);
                pos_pair_list_free(ppl_tmp);
            }
            break;

        case QUES:
            rfa_construct(re->left, rfa, first);
            if (re->pos) {
                if (!NULLABLE(first)) {
                    pos_pair_insert_pos_after(GAMMA_POS, first->sentinal->prev);
                    first->len++;
                }
            } else {
                pos_pair_list_remove(first, GAMMA_POS);
                pos_pair_insert_pos_after(GAMMA_POS, first->sentinal);
                first->len++;
            }
            break;

        /* TODO: */
        case COUNTER: break;
        case LOOKAHEAD: break;
    }
}

static void rfa_absorb(Rfa *rfa, size_t pos, int *visited)
{
    PosPair     *t, *e, *tmp, *pp, *p = NULL;
    PosPairList *follow, *follow_pos = rfa->follow[pos];

    visited[pos] = TRUE;
    FOREACH(t, follow_pos) {
        if (!visited[t->pos]) rfa_absorb(rfa, t->pos, visited);
    }

    e = follow_pos->sentinal->next;
    while (e != follow_pos->sentinal) {
        if (e->pos == pos) {
            if (IS_NONEPS_TRANSITION(rfa->positions[e->pos])) p = e;
            e = e->next;
            continue;
        } else if (IS_NONEPS_TRANSITION(rfa->positions[e->pos])) {
            e = e->next;
            continue;
        }

        tmp    = e;
        follow = rfa->follow[e->pos];
        FOREACH(t, follow) {
            FOREACH(pp, follow_pos) {
                if (pp->pos == e->pos || pp->pos == t->pos) break;
            }
            /* since e is in follow_pos, we should at least find e and thus no
             * need to check if pp is follow_pos->sentinal */
            if (pp->pos != e->pos) continue;

            if (pos_pair_remove_from(t->pos, tmp, follow_pos->sentinal))
                follow_pos->len--;
            pp = pos_pair_clone(t);
            pos_pair_action_prepend(pp, action_new(e->pos, rfa->positions));
            pos_pair_insert_after(pp, tmp);
            follow_pos->len++;
            tmp = pp;
        }

        e = pos_pair_remove(e);
        follow_pos->len--;
    }

    if (p) {
        for (t = p->next; t != follow_pos->sentinal; t = t->next) {
            pp = pos_pair_clone(p);
            pos_pair_action_append(pp, action_new(pos, rfa->positions));

            /* prepend pp action list (in its order) to t's action list */
            pp->last_action->next = t->first_action;
            t->first_action       = pp->first_action;
            if (t->last_action == NULL) t->last_action = pp->last_action;
            t->nactions      += pp->nactions;
            pp->first_action = pp->last_action = NULL;
            pos_pair_free(pp);
        }
        pos_pair_remove(p);
        follow_pos->len--;
    }

    visited[pos] = FALSE;
}

/* TODO: */
static len_t rfa_insts_len(Rfa *rfa)
{
    len_t insts_len = 0;

    return insts_len;
}

static byte *emit(const Rfa *rfa, byte *pc, Program *prog) { return pc; }
