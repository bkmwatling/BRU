#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glushkov.h"
#include "smir.h"

#define IS_EMPTY(ppl) ((ppl)->sentinel->next == (ppl)->sentinel)

#define FOREACH(elem, sentinel) \
    for ((elem) = (sentinel)->next; (elem) != (sentinel); (elem) = (elem)->next)

#define FOREACH_REV(elem, sentinel) \
    for ((elem) = (sentinel)->prev; (elem) != (sentinel); (elem) = (elem)->prev)

#define IS_EPS_TRANSITION(act) \
    ((act) != NULL && smir_action_type((act)) == ACT_SAVE)

#define APPEND_GAMMA(ppl)                                    \
    do {                                                     \
        pp_insert_pos_after(ppl->sentinel->prev, GAMMA_POS); \
        ppl->gamma = ppl->sentinel->prev;                    \
        ppl->len++;                                          \
    } while (0)

#define PREPEND_GAMMA(ppl)                             \
    do {                                               \
        pp_insert_pos_after(ppl->sentinel, GAMMA_POS); \
        ppl->gamma = ppl->sentinel->next;              \
        ppl->len++;                                    \
    } while (0)

/* START_POS and GAMMA_POS should be the same since gamma isn't an actual
 * position and the start position/state should not follow from others        */
#define START_POS 0
#define GAMMA_POS 0

#define FIRST(rfa)      ((rfa)->follow[START_POS])
#define NULLABLE(first) ((first)->gamma != NULL)

#define RFA_NEW_FROM(rfa) \
    rfa_new((rfa)->follow, (rfa)->npositions, (rfa)->positions)

typedef struct pos_pair PosPair;

struct pos_pair {
    size_t      pos;     /*<< linearised position for Glushkov                */
    ActionList *actions; /*<< list of actions on transition                   */
    PosPair    *prev;
    PosPair    *next;
};

typedef struct {
    size_t   len;      /*<< length of (number of elements in) the list        */
    PosPair *sentinel; /*<< sentinel for circly linked list                   */
    PosPair *gamma;    /*<< shortcut to gamma pair in linked list             */
} PosPairList;

typedef struct {
    PosPairList  *last;       /*<< last *set* for Glushkov                    */
    PosPairList **follow;     /*<< integer map of positions to follow lists   */
    size_t        npositions; /*<< number of linearised positions             */
    const Action **positions; /*<< integer map of positions to actual info    */
} Rfa;

static void     pp_free(PosPair *self);
static PosPair *pp_clone(const PosPair *self);
static void     pp_action_push_front(PosPair *self, const Action *action);
static void     pp_insert_after(PosPair *self, PosPair *pp);
static PosPair *pp_insert_pos_after(PosPair *self, size_t pos);
static PosPair *pp_remove(PosPair *self);
static int      pp_remove_from(size_t pos, PosPair *start, PosPair *end);

static PosPairList *ppl_new(void);
static void         ppl_free(PosPairList *self);
static PosPairList *ppl_clone(const PosPairList *self);
static void         ppl_clear(PosPairList *self);
static void         ppl_remove(PosPairList *self, size_t pos);
static void         ppl_replace_gamma(PosPairList *self, PosPairList *ppl);
static void         ppl_append(PosPairList *self, PosPairList *ppl);

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Action **positions);
static void rfa_free(Rfa *self);
static void rfa_construct(Rfa *self, const RegexNode *re, PosPairList *first);
static void rfa_merge_outgoing(Rfa *rfa, size_t pos, len_t *visited);
static void
rfa_absorb(Rfa *self, size_t pos, len_t *visited, const CompilerOpts *opts);

static size_t count(const RegexNode *re);
static void   emit(StateMachine *sm, const Rfa *rfa);

#define DEBUG_GLUSHKOV
#if defined(DEBUG) || defined(DEBUG_GLUSHKOV)
static void ppl_print(PosPairList *self, FILE *stream);
static void rfa_print(Rfa *self, FILE *stream);
#else
#    define ppl_print(...)
#    define rfa_print(...)
#endif

/* --- Main Routine --------------------------------------------------------- */

StateMachine *glushkov_construct(Regex re, const CompilerOpts *opts)
{
    len_t          npositions;
    size_t         i;
    PosPair       *pp;
    PosPairList  **follow;
    const Action **positions;
    len_t         *visited;
    Rfa           *rfa;
    StateMachine  *sm;

    npositions = count(re.root) + 1; /* + 1 so that 0 is gamma/start */
    // NOLINTBEGIN(bugprone-sizeof-expression)
    positions  = malloc(npositions * sizeof(*positions));
    follow     = malloc(npositions * sizeof(*follow));
    // NOLINTEND(bugprone-sizeof-expression)
    visited    = calloc(npositions, sizeof(*visited));
    for (i = 0; i < npositions; i++) follow[i] = ppl_new();

    positions[START_POS] = NULL;
    rfa                  = rfa_new(follow, 1, positions);
    rfa_construct(rfa, re.root, NULL);
    rfa_print(rfa, stderr);
    rfa_merge_outgoing(rfa, START_POS, visited);
    rfa_print(rfa, stderr);
    rfa_absorb(rfa, START_POS, visited, opts);
    rfa_print(rfa, stderr);

    // TODO: get correct number of positions after absorb
    sm = smir_new(re.regex, rfa->npositions - 1); // don't want initial state
    emit(sm, rfa);

    rfa_free(rfa);
    for (i = 0; i < npositions; i++) {
        // manually free just ppl and pp containers not actions
        while (follow[i]->sentinel->next != follow[i]->sentinel) {
            pp                        = follow[i]->sentinel->next;
            follow[i]->sentinel->next = pp->next;
            free(pp);
        }

        pp_free(follow[i]->sentinel);
        free(follow[i]);
    }
    free(follow);
    free(positions);
    free(visited);

    return sm;
}

static PosPair *pos_pair_new(size_t pos)
{
    PosPair *pp = malloc(sizeof(*pp));

    pp->pos     = pos;
    pp->actions = smir_action_list_new();
    pp->prev = pp->next = NULL;

    return pp;
}

static void pp_free(PosPair *self)
{
    smir_action_list_free(self->actions);
    self->prev = self->next = NULL;
    free(self);
}

static PosPair *pp_clone(const PosPair *self)
{
    PosPair *pp = malloc(sizeof(*pp));

    pp->pos     = self->pos;
    pp->actions = smir_action_list_clone(self->actions);
    pp->prev = pp->next = NULL;

    return pp;
}

static void pp_action_push_front(PosPair *self, const Action *action)
{
    const Action       *act, *a;
    ActionListIterator *iter = smir_action_list_iter(self->actions);
    size_t              action_idx, act_idx;

    while ((act = smir_action_list_iterator_next(iter))) {
        if (smir_action_type(action) != smir_action_type(act)) continue;
        switch (smir_action_type(action)) {
            case ACT_BEGIN: /* fallthrough */
            case ACT_END: smir_action_free(action); goto cleanup;

            case ACT_MEMO:
                if (smir_action_get_num(action) == smir_action_get_num(act)) {
                    smir_action_free(action);
                    goto cleanup;
                }

            case ACT_SAVE:
                action_idx = smir_action_get_num(action);
                act_idx    = smir_action_get_num(act);
                if (action_idx == act_idx) {
                    smir_action_free(action);
                    goto cleanup;
                } else if (action_idx % 2 == 1 && act_idx == action_idx - 1) {
                    goto push_front;
                } else if (action_idx % 2 == 0 && act_idx == action_idx + 1) {
                    while ((a = smir_action_list_iterator_next(iter)) &&
                           (smir_action_type(a) != ACT_SAVE ||
                            action_idx != smir_action_get_num(a)))
                        ;
                    if (!a) goto push_front;
                    // make iterator go back to act
                    while ((a = smir_action_list_iterator_prev(iter)) &&
                           a != act)
                        ;
                    smir_action_list_iterator_remove(iter);
                    smir_action_free(action);
                    goto cleanup;
                }
                break;

            case ACT_CHAR:   /* fallthrough */
            case ACT_PRED:   /* fallthrough */
            case ACT_EPSCHK: /* fallthrough */
            case ACT_EPSSET: assert(0 && "unreachable");
        }
    }

push_front:
    smir_action_list_push_front(self->actions, action);

cleanup:
    free(iter);
}

static void pp_insert_after(PosPair *self, PosPair *pp)
{
    pp->prev       = self;
    pp->next       = self->next;
    self->next     = pp;
    pp->next->prev = pp;
}

static PosPair *pp_insert_pos_after(PosPair *self, size_t pos)
{
    PosPair *pp = pos_pair_new(pos);
    pp_insert_after(self, pp);
    return pp;
}

static PosPair *pp_remove(PosPair *self)
{
    PosPair *next = self->next;

    if (self->prev) self->prev->next = self->next;
    if (self->next) self->next->prev = self->prev;
    self->prev = self->next = NULL;

    pp_free(self);
    return next;
}

static int pp_remove_from(size_t pos, PosPair *start, PosPair *end)
{
    PosPair *pp;

    for (pp = start; pp && pp != end; pp = pp->next) {
        if (pp->pos == pos) {
            pp_remove(pp);
            return TRUE;
        }
    }

    return FALSE;
}

static PosPairList *ppl_new(void)
{
    PosPairList *ppl = malloc(sizeof(*ppl));

    ppl->len            = 0;
    ppl->sentinel       = pos_pair_new(START_POS);
    ppl->sentinel->prev = ppl->sentinel->next = ppl->sentinel;
    ppl->gamma                                = NULL;

    return ppl;
}

static void ppl_free(PosPairList *self)
{
    ppl_clear(self);
    pp_free(self->sentinel);
    free(self);
}

static PosPairList *ppl_clone(const PosPairList *self)
{
    PosPairList *ppl = ppl_new();
    PosPair     *pp;

    ppl->len = self->len;
    FOREACH(pp, self->sentinel) {
        pp_insert_after(ppl->sentinel->prev, pp_clone(pp));
        if (pp == self->gamma) ppl->gamma = ppl->sentinel->prev;
    }

    return ppl;
}

static void ppl_clear(PosPairList *self)
{
    PosPair *pp;

    while (self->sentinel->next != self->sentinel) {
        pp                   = self->sentinel->next;
        self->sentinel->next = pp->next;
        pp->prev = pp->next = NULL;
        pp_free(pp);
    }

    self->sentinel->prev = self->sentinel->next = self->sentinel;
    self->len                                   = 0;
    self->gamma                                 = NULL;
}

static void ppl_remove(PosPairList *self, size_t pos)
{
    PosPair *pp;

    if (pos == GAMMA_POS) {
        pp = self->gamma ? self->gamma : self->sentinel;
    } else {
        FOREACH(pp, self->sentinel) {
            if (pp->pos == pos) break;
        }
    }

    if (pp == self->sentinel) return;

    pp_remove(pp);
    self->len--;
    if (pos == GAMMA_POS) self->gamma = NULL;
}

static void ppl_replace_gamma(PosPairList *self, PosPairList *ppl)
{
    if (self->gamma == NULL) return;
    if (IS_EMPTY(ppl)) {
        ppl_remove(self, GAMMA_POS);
        return;
    }

    self->gamma->prev->next   = ppl->sentinel->next;
    ppl->sentinel->next->prev = self->gamma->prev;
    ppl->sentinel->prev->next = self->gamma->next;
    self->gamma->next->prev   = ppl->sentinel->prev;
    self->gamma->prev = self->gamma->next  = NULL;
    self->len                             += ppl->len - 1;
    ppl->sentinel->prev = ppl->sentinel->next = ppl->sentinel;
    ppl->len                                  = 0;

    pp_free(self->gamma);
    self->gamma = ppl->gamma;
    ppl->gamma  = NULL;
}

static void ppl_append(PosPairList *self, PosPairList *ppl)
{
    if (IS_EMPTY(ppl)) return;

    self->sentinel->prev->next  = ppl->sentinel->next;
    ppl->sentinel->next->prev   = self->sentinel->prev;
    ppl->sentinel->prev->next   = self->sentinel;
    self->sentinel->prev        = ppl->sentinel->prev;
    self->len                  += ppl->len;
    ppl->sentinel->prev = ppl->sentinel->next = ppl->sentinel;
    ppl->len                                  = 0;

    if (self->gamma == NULL) self->gamma = ppl->gamma;
    ppl->gamma = NULL;
}

static Rfa *
rfa_new(PosPairList **follow, size_t npositions, const Action **positions)
{
    Rfa *rfa = malloc(sizeof(*rfa));

    rfa->last       = ppl_new();
    rfa->follow     = follow;
    rfa->npositions = npositions;
    rfa->positions  = positions;

    return rfa;
}

static void rfa_free(Rfa *self)
{
    ppl_free(self->last);
    free(self);
}

static void rfa_construct(Rfa *self, const RegexNode *re, PosPairList *first)
{
#define APPEND_POSITION(action)                               \
    do {                                                      \
                                                              \
        self->positions[pos = self->npositions++] = (action); \
        PREPEND_GAMMA(self->follow[pos]);                     \
        pp_insert_pos_after(first->sentinel, pos);            \
        first->len++;                                         \
        pp_insert_pos_after(self->last->sentinel, pos);       \
        self->last->len++;                                    \
    } while (0)

    Rfa         *rfa_tmp;
    PosPairList *ppl_tmp, *first_tmp;
    PosPair     *pp;
    size_t       pos = 0;
    size_t       pos_open, pos_close; /* for capture positions */

    if (first == NULL) first = FIRST(self);
    switch (re->type) {
        case CARET: APPEND_POSITION(smir_action_zwa(ACT_BEGIN)); break;
        case DOLLAR: APPEND_POSITION(smir_action_zwa(ACT_END)); break;
        case LITERAL: APPEND_POSITION(smir_action_char(re->ch)); break;
        case MEMOISE:
            APPEND_POSITION(smir_action_num(ACT_MEMO, re->rid));
            break;
        case CC:
            APPEND_POSITION(smir_action_predicate(re->intervals, re->cc_len));
            break;

        case ALT:
            rfa_construct(self, re->left, first);
            rfa_tmp   = RFA_NEW_FROM(self);
            first_tmp = ppl_new();
            rfa_construct(rfa_tmp, re->right, first_tmp);
            self->npositions = rfa_tmp->npositions;

            if (NULLABLE(first)) ppl_remove(first_tmp, GAMMA_POS);
            ppl_append(first, first_tmp);
            ppl_append(self->last, rfa_tmp->last);

            ppl_free(first_tmp);
            rfa_free(rfa_tmp);
            break;

        case CONCAT:
            rfa_construct(self, re->left, first);
            rfa_tmp   = RFA_NEW_FROM(self);
            first_tmp = ppl_new();
            rfa_construct(rfa_tmp, re->right, first_tmp);
            self->npositions = rfa_tmp->npositions;

            FOREACH(pp, self->last->sentinel) {
                ppl_tmp = ppl_clone(first_tmp);
                ppl_replace_gamma(self->follow[pp->pos], ppl_tmp);
                ppl_free(ppl_tmp);
            }

            if (NULLABLE(first)) ppl_replace_gamma(first, first_tmp);

            ppl_tmp       = self->last;
            self->last    = rfa_tmp->last;
            rfa_tmp->last = ppl_tmp;
            if (NULLABLE(first_tmp)) ppl_append(self->last, rfa_tmp->last);

            ppl_free(first_tmp);
            rfa_free(rfa_tmp);
            break;

        case CAPTURE:
            self->positions[pos_open = self->npositions++] =
                smir_action_num(ACT_SAVE, 2 * re->capture_idx);

            rfa_construct(self, re->left, first);

            self->positions[pos_close = self->npositions++] =
                smir_action_num(ACT_SAVE, 2 * re->capture_idx + 1);

            ppl_append(self->follow[pos_open], first);
            /* allows next loop to handle gamma replace */
            if (NULLABLE(self->follow[pos_open])) {
                pp_insert_pos_after(self->last->sentinel, pos_open);
                self->last->len++;
            }
            FOREACH(pp, self->last->sentinel) {
                self->follow[pp->pos]->gamma->pos = pos_close;
                self->follow[pp->pos]->gamma      = NULL;
            }
            PREPEND_GAMMA(self->follow[pos_close]);
            ppl_clear(self->last);

            pp_insert_pos_after(first->sentinel, pos_open);
            first->len++;
            pp_insert_pos_after(self->last->sentinel, pos_close);
            self->last->len++;

            break;

        case STAR:
            rfa_construct(self, re->left, first);
            if (re->pos) {
                if (!NULLABLE(first)) APPEND_GAMMA(first);
            } else {
                ppl_remove(first, GAMMA_POS);
                PREPEND_GAMMA(first);
            }

            FOREACH(pp, self->last->sentinel) {
                ppl_tmp = ppl_clone(first);
                ppl_replace_gamma(self->follow[pp->pos], ppl_tmp);
                ppl_free(ppl_tmp);
            }
            break;

        case PLUS:
            rfa_construct(self, re->left, first);
            FOREACH(pp, self->last->sentinel) {
                ppl_tmp = ppl_clone(first);
                if (re->pos) {
                    if (!NULLABLE(first)) APPEND_GAMMA(ppl_tmp);
                } else {
                    ppl_remove(ppl_tmp, GAMMA_POS);
                    PREPEND_GAMMA(ppl_tmp);
                }
                ppl_replace_gamma(self->follow[pp->pos], ppl_tmp);
                ppl_free(ppl_tmp);
            }
            break;

        case QUES:
            rfa_construct(self, re->left, first);
            if (re->pos) {
                if (!NULLABLE(first)) APPEND_GAMMA(first);
            } else {
                ppl_remove(first, GAMMA_POS);
                PREPEND_GAMMA(first);
            }
            break;

        /* TODO: */
        case COUNTER:
        case LOOKAHEAD: assert(0 && "TODO");
        case NREGEXTYPES: assert(0 && "unreachable");
    }

#undef APPEND_POSITION
}

static void rfa_merge_outgoing(Rfa *rfa, size_t pos, len_t *visited)
{
    len_t       *seen;
    PosPair     *t, *e;
    PosPairList *follow = rfa->follow[pos];

    visited[pos] = TRUE;
    FOREACH(t, follow->sentinel)
        if (!visited[t->pos]) rfa_merge_outgoing(rfa, t->pos, visited);

    e    = follow->sentinel->next;
    seen = calloc(rfa->npositions, sizeof(*seen));
    while (e != follow->sentinel) {
        if (seen[e->pos]) {
            e = pp_remove(e);
            follow->len--;
        } else {
            seen[e->pos] = TRUE;
            e            = e->next;
        }
    }
    free(seen);
    visited[pos] = FALSE;
}

static void
rfa_absorb(Rfa *rfa, size_t pos, len_t *visited, const CompilerOpts *opts)
{
    PosPair            *t, *e, *tmp, *pp, *p = NULL;
    PosPairList        *follow, *follow_pos = rfa->follow[pos];
    ActionListIterator *iter;
    const Action       *act;

    visited[pos] = TRUE;
    FOREACH(t, follow_pos->sentinel)
        if (!visited[t->pos]) rfa_absorb(rfa, t->pos, visited, opts);

    e = follow_pos->sentinel->next;
    while (e != follow_pos->sentinel) {
        if (e->pos == pos && pos != GAMMA_POS) {
            if (IS_EPS_TRANSITION(rfa->positions[e->pos])) p = e;
            e = e->next;
            continue;
        } else if (!IS_EPS_TRANSITION(rfa->positions[e->pos])) {
            e = e->next;
            continue;
        }

        tmp    = e;
        follow = rfa->follow[e->pos];
        FOREACH(t, follow->sentinel) {
            // iterate over edges (pos, pp) up to (pos, e)
            // looking for an edge (pos, t).
            // if we find (pos, t), then current value of t can
            // be skipped since it will have a lower priority than
            // the one we found.
            FOREACH(pp, follow_pos->sentinel)
                if (pp->pos == e->pos || pp->pos == t->pos) break;
            /* since e is in follow_pos, we should at least find e and thus no
             * need to check if pp is follow_pos->sentinel */
            if (pp->pos != e->pos) continue;

            // otherwise, iterate over edges (pos, e) up to the end,
            // and if any of them are t, delete them as they will have a lower
            // priority than the current one
            if (pp_remove_from(t->pos, tmp, follow_pos->sentinel))
                follow_pos->len--;
            pp = pp_clone(t);
            pp_action_push_front(pp, smir_action_clone(rfa->positions[e->pos]));
            iter = smir_action_list_iter(e->actions);
            while ((act = smir_action_list_iterator_prev(iter)))
                pp_action_push_front(pp, smir_action_clone(act));
            free(iter);
            pp_insert_after(tmp, pp);
            follow_pos->len++;
            tmp = pp;
        }

        e = pp_remove(e);
        follow_pos->len--;
    }

    if (p) {
        if (opts->capture_semantics == CS_PCRE) {
            for (t = p->next; t != follow_pos->sentinel; t = t->next) {
                pp_action_push_front(t, smir_action_clone(rfa->positions[pos]));
                iter = smir_action_list_iter(p->actions);
                while ((act = smir_action_list_iterator_prev(iter)))
                    pp_action_push_front(t, smir_action_clone(act));
                free(iter);
            }
        }
        pp_remove(p);
        follow_pos->len--;
    }

    visited[pos] = FALSE;
}

static size_t count(const RegexNode *re)
{
    size_t npos = 0;

    switch (re->type) {
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case MEMOISE: /* fallthrough */
        case LITERAL: /* fallthrough */
        case CC: npos = 1; break;

        case ALT: /* fallthrough */
        case CONCAT: npos = count(re->left) + count(re->right); break;

        case CAPTURE: npos = 2 + count(re->left); break;

        case STAR: /* fallthrough */
        case PLUS: /* fallthrough */
        case QUES: npos = count(re->left); break;

        /* TODO: neither work yet */
        case COUNTER: npos = count(re->left); break;
        case LOOKAHEAD: npos = 1 + count(re->left); break;

        case NREGEXTYPES: assert(0 && "unreachable");
    }

    return npos;
}

static void emit(StateMachine *sm, const Rfa *rfa)
{
    PosPair *pp;
    trans_id tid;
    size_t   i;

    FOREACH(pp, FIRST(rfa)->sentinel) {
        tid = smir_set_initial(sm, pp->pos);
        smir_trans_set_actions(sm, tid, pp->actions);
    }

    for (i = 1; i < rfa->npositions; i++) {
        fprintf(stderr, "i = %lu, npos = %lu, nstates = %lu\n", i,
                rfa->npositions, smir_get_num_states(sm));
        smir_state_append_action(sm, i, rfa->positions[i]);
        FOREACH(pp, rfa->follow[i]->sentinel) {
            tid = smir_add_transition(sm, i);
            smir_set_dst(sm, tid, pp->pos);
            smir_trans_set_actions(sm, tid, pp->actions);
        }
    }
}

/* --- Debug functions ------------------------------------------------------ */

#if defined(DEBUG) || defined(DEBUG_GLUSHKOV)
static void ppl_print(PosPairList *self, FILE *stream)
{
    PosPair            *pp;
    const Action       *act;
    ActionListIterator *iter;
    size_t              act_idx;

    FOREACH(pp, self->sentinel) {
        fprintf(stream, "%lu (", pp->pos);
        iter = smir_action_list_iter(pp->actions);
        while ((act = smir_action_list_iterator_next(iter))) {
            act_idx = smir_action_get_num(act);
            switch (smir_action_type(act)) {
                case ACT_BEGIN: fprintf(stream, "^"); break;
                case ACT_END: fprintf(stream, "$"); break;
                case ACT_MEMO: fprintf(stream, "#"); break;

                case ACT_SAVE:
                    fprintf(stream, "%c_%lu", act_idx % 2 == 0 ? '[' : ']',
                            act_idx / 2);
                    break;

                default:
                    fprintf(stream, "action type = %d\n",
                            smir_action_type(act));
                    assert(0 && "unreachable");
            }
            fprintf(stream, ",");
        }
        fprintf(stream, ") ");
        free(iter);
    }
}

static void rfa_print(Rfa *rfa, FILE *stream)
{
    size_t i;

    fprintf(stream, "\nRFA FOLLOW SET:\n");
    for (i = 0; i < rfa->npositions; i++) {
        fprintf(stream, "%lu: ", i);
        ppl_print(rfa->follow[i], stream);
        fprintf(stream, "\n");
    }
    fprintf(stream, "\n");
}
#endif
