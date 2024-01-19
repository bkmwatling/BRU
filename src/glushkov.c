#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stc/fatp/vec.h"

#include "glushkov.h"

#define SET_OFFSET(p, pc) (*(p) = (pc) - (byte *) ((p) + 1))

#define SPLIT_LABELS_PTRS(p, q, re, pc)                              \
    (p) = (offset_t *) ((re)->pos ? (pc) : (pc) + sizeof(offset_t)); \
    (q) = (offset_t *) ((re)->pos ? (pc) + sizeof(offset_t) : (pc))

#define IS_EMPTY(ppl) ((ppl)->sentinel->next == (ppl)->sentinel)

#define FOREACH(elem, sentinel) \
    for ((elem) = (sentinel)->next; (elem) != (sentinel); (elem) = (elem)->next)

#define FOREACH_REV(elem, sentinel) \
    for ((elem) = (sentinel)->prev; (elem) != (sentinel); (elem) = (elem)->prev)

#define IS_EPS_TRANSITION(re) ((re) != NULL && (re)->type == CAPTURE)

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

typedef struct action Action;

struct action {
    const Regex *re;
    Action      *prev;
    Action      *next;
};

typedef struct pos_pair PosPair;

struct pos_pair {
    size_t   pos;             /*<< linearised position for Glushkov           */
    len_t    nactions;        /*<< number of actions for transition           */
    Action  *action_sentinel; /*<< sentinel node for action list              */
    PosPair *prev;
    PosPair *next;
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
    const Regex **positions;  /*<< integer map of positions to actual info    */
} Rfa;

static Action *action_new(size_t pos, const Regex **positions);
static void    action_free(Action *self);
static Action *action_clone(const Action *self);

static void     pp_free(PosPair *self);
static PosPair *pp_clone(const PosPair *self);
static void     pp_action_prepend(PosPair *self, Action *action);
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
rfa_new(PosPairList **follow, size_t npositions, const Regex **positions);
static void rfa_free(Rfa *self);
static void rfa_construct(Rfa *self, const Regex *re, PosPairList *first);
static void rfa_merge_outgoing(Rfa *rfa, size_t pos, len_t *visited);
static void
rfa_absorb(Rfa *self, size_t pos, len_t *visited, const CompilerOpts *opts);
static len_t rfa_insts_len(Rfa *self, len_t *pc_map);

static size_t
count(const Regex *re, len_t *aux_len, len_t *ncaptures, len_t *ncounters);
static byte *emit(const Rfa *rfa, byte *pc, Program *prog, len_t *pc_map);
static len_t
emit_transition(PosPair *pp, byte **pc, Program *prog, len_t *pc_map);

#if defined(DEBUG) || defined(DEBUG_GLUSHKOV)
static void ppl_print(PosPairList *self, FILE *stream);
static void rfa_print(Rfa *self, FILE *stream);
#else
#    define ppl_print(...)
#    define rfa_print(...)
#endif

const Program *glushkov_compile(const Regex *re, const CompilerOpts *opts)
{
    len_t         npositions, aux_len = 0, ncaptures = 0, ncounters = 0;
    size_t        i;
    PosPairList **follow;
    const Regex **positions;
    len_t        *visited;
    Rfa          *rfa;
    Program      *prog;

    /* + 1 so that 0 is gamma/start */
    npositions = count(re, &aux_len, &ncaptures, &ncounters) + 1;
    positions  = malloc(npositions * sizeof(Regex *));
    follow     = malloc(npositions * sizeof(PosPairList *));
    visited    = malloc(npositions * sizeof(int));
    memset(visited, 0, npositions * sizeof(int));
    for (i = 0; i < npositions; i++) follow[i] = ppl_new();

    positions[START_POS] = NULL;
    rfa                  = rfa_new(follow, 1, positions);
    rfa_construct(rfa, re, NULL);
    rfa_print(rfa, stderr);
    rfa_merge_outgoing(rfa, START_POS, visited);
    rfa_print(rfa, stderr);
    rfa_absorb(rfa, START_POS, visited, opts);
    rfa_print(rfa, stderr);

    prog = program_new(rfa_insts_len(rfa, visited), aux_len, ncounters, 0,
                       ncaptures);

    emit(rfa, prog->insts, prog, visited);

    rfa_free(rfa);
    for (i = 0; i < npositions; i++) {
        ppl_free(follow[i]);
        if (i > 0 && positions[i]->type == CAPTURE)
            free((Regex *) positions[i]);
    }
    free(follow);
    free(positions);
    free(visited);

    return prog;
}

static Action *action_new(size_t pos, const Regex **positions)
{
    Action      *action = malloc(sizeof(Action));
    const Regex *re     = positions ? positions[pos] : NULL;

    action->re   = re;
    action->prev = action->next = NULL;

    return action;
}

static void action_free(Action *self)
{
    self->prev = self->next = NULL;
    free(self);
}

static Action *action_clone(const Action *self)
{
    Action *act = malloc(sizeof(Action));

    act->re   = self->re;
    act->prev = act->next = NULL;

    return act;
}

static PosPair *pos_pair_new(size_t pos)
{
    PosPair *pp = malloc(sizeof(PosPair));

    pp->pos                   = pos;
    pp->nactions              = 0;
    pp->action_sentinel       = action_new(START_POS, NULL);
    pp->action_sentinel->prev = pp->action_sentinel->next = pp->action_sentinel;
    pp->prev = pp->next = NULL;

    return pp;
}

static void pp_free(PosPair *self)
{
    Action *action;

    while (self->action_sentinel->next != self->action_sentinel) {
        action                      = self->action_sentinel->next;
        self->action_sentinel->next = action->next;
        action->prev = action->next = NULL;
        action_free(action);
    }

    self->nactions = 0;
    action_free(self->action_sentinel);
    self->prev = self->next = NULL;
    free(self);
}

static PosPair *pp_clone(const PosPair *self)
{
    Action  *act, *sentinel = self->action_sentinel;
    PosPair *pp = pos_pair_new(self->pos);

    FOREACH_REV(act, sentinel) pp_action_prepend(pp, action_clone(act));

    return pp;
}

static void pp_action_prepend(PosPair *self, Action *action)
{
    Action *act, *a;

    FOREACH(act, self->action_sentinel) {
        if (action->re->type != act->re->type) continue;
        switch (action->re->type) {
            case CARET:   /* fallthrough */
            case MEMOISE: /* fallthrough */
            case DOLLAR: action_free(action); return;
            case CAPTURE:
                if (action->re->capture_idx == act->re->capture_idx) {
                    action_free(action);
                    return;
                } else if (action->re->capture_idx % 2 == 1 &&
                           act->re->capture_idx ==
                               action->re->capture_idx - 1) {
                    goto prepend;
                } else if (action->re->capture_idx % 2 == 0 &&
                           act->re->capture_idx ==
                               action->re->capture_idx + 1) {
                    for (a = act->next;
                         a != self->action_sentinel &&
                         (a->re->type != CAPTURE ||
                          action->re->capture_idx != a->re->capture_idx);
                         a = a->next)
                        ;
                    if (a == self->action_sentinel) goto prepend;
                    act->prev->next = act->next;
                    act->next->prev = act->prev;
                    act->prev = act->next = NULL;
                    action_free(act);
                    action_free(action);
                    return;
                } else {
                    break;
                }
            default: assert(0 && "unreachable");
        }
    }

prepend:
    action->prev                = self->action_sentinel;
    action->next                = self->action_sentinel->next;
    self->action_sentinel->next = action;
    action->next->prev          = action;
    self->nactions++;
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
    PosPairList *ppl = malloc(sizeof(PosPairList));

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
rfa_new(PosPairList **follow, size_t npositions, const Regex **positions)
{
    Rfa *rfa = malloc(sizeof(Rfa));

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

static void rfa_construct(Rfa *self, const Regex *re, PosPairList *first)
{
    Regex       *re_tmp;
    Rfa         *rfa_tmp;
    PosPairList *ppl_tmp, *first_tmp;
    PosPair     *pp;
    size_t       pos = 0;
    size_t       pos_open, pos_close; /* for capture positions */

    if (first == NULL) first = FIRST(self);
    switch (re->type) {
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case LITERAL: /* fallthrough */
        case MEMOISE: /* fallthrough */
        case CC:
            pos                  = self->npositions++;
            self->positions[pos] = re;
            PREPEND_GAMMA(self->follow[pos]);
            pp_insert_pos_after(first->sentinel, pos);
            first->len++;
            pp_insert_pos_after(self->last->sentinel, pos);
            self->last->len++;
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
            pos_open = self->npositions++;
            /* make copy of re to set capture_idx using strategy for VM */
            re_tmp   = malloc(sizeof(Regex));
            memcpy(re_tmp, re, sizeof(Regex));
            re_tmp->capture_idx       *= 2;
            self->positions[pos_open]  = re_tmp;

            rfa_construct(self, re->left, first);

            pos_close = self->npositions++;
            /* make copy of re to set capture_idx using strategy for VM */
            re_tmp    = malloc(sizeof(Regex));
            memcpy(re_tmp, re, sizeof(Regex));
            re_tmp->capture_idx        = re_tmp->capture_idx * 2 + 1;
            self->positions[pos_close] = re_tmp;

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
        case COUNTER: break;
        case LOOKAHEAD: break;
        case NREGEXTYPES: assert(0 && "unreachable");
    }
}

static void rfa_merge_outgoing(Rfa *rfa, size_t pos, len_t *visited)
{
    len_t       *seen;
    PosPair     *t, *e;
    PosPairList *follow = rfa->follow[pos];

    visited[pos] = TRUE;
    FOREACH(t, follow->sentinel) {
        if (!visited[t->pos]) rfa_merge_outgoing(rfa, t->pos, visited);
    }

    e    = follow->sentinel->next;
    seen = calloc(rfa->npositions, sizeof(len_t));
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
    PosPair     *t, *e, *tmp, *pp, *p = NULL;
    PosPairList *follow, *follow_pos = rfa->follow[pos];
    Action      *act;

    visited[pos] = TRUE;
    FOREACH(t, follow_pos->sentinel) {
        if (!visited[t->pos]) rfa_absorb(rfa, t->pos, visited, opts);
    }

    e = follow_pos->sentinel->next;
    while (e != follow_pos->sentinel) {
        fprintf(stderr, "processing (%zu, %zu)\n", pos, e->pos);
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
            FOREACH(pp, follow_pos->sentinel) {
                if (pp->pos == e->pos || pp->pos == t->pos) break;
            }
            /* since e is in follow_pos, we should at least find e and thus no
             * need to check if pp is follow_pos->sentinel */
            if (pp->pos != e->pos) continue;

            // otherwise, iterate over edges (pos, e) up to the end,
            // and if any of them are t, delete them as they will have a lower
            // priority than the current one
            if (pp_remove_from(t->pos, tmp, follow_pos->sentinel)) {
                fprintf(stderr, "removed %zu\n", t->pos);
                follow_pos->len--;
            }
            pp = pp_clone(t);
            pp_action_prepend(pp, action_new(e->pos, rfa->positions));
            FOREACH_REV(act, e->action_sentinel)
            pp_action_prepend(pp, action_clone(act));
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
                pp_action_prepend(t, action_new(pos, rfa->positions));
                FOREACH_REV(act, p->action_sentinel)
                {
                    pp_action_prepend(t, action_clone(act));
                }
            }
        }
        pp_remove(p);
        follow_pos->len--;
    }

    visited[pos] = FALSE;
}

/* TODO: */
static len_t rfa_insts_len(Rfa *rfa, len_t *pc_map)
{
    len_t        insts_len = 0;
    byte        *visited   = malloc(rfa->npositions * sizeof(byte));
    size_t      *states    = NULL;
    size_t       pos;
    PosPairList *follow;
    PosPair     *pp;
    const Regex *re;

    /* initialise processing */
    memset(visited, 0, rfa->npositions * sizeof(byte));
    stc_vec_init(states, rfa->npositions);
    stc_vec_push(states, START_POS);
    visited[START_POS] = TRUE;

    /* process each state in states (possibly adding more states) */
    while (stc_vec_len_unsafe(states) > 0) {
        pos = stc_vec_pop(states);
        if (pos != START_POS) {
            pc_map[pos] = insts_len;
            switch ((re = rfa->positions[pos])->type) {
                case LITERAL:
                    insts_len += sizeof(byte) + sizeof(const char *);
                    break;
                case CC: insts_len += sizeof(byte) + 2 * sizeof(len_t); break;
                case MEMOISE:
                case CARET:
                case DOLLAR: insts_len += sizeof(byte); break;
                default: assert(0 && "unreachable");
            }
        }

        follow = rfa->follow[pos];
        /* allocate space for twsitch */
        /* TODO: optimise for actionless transitions */
        if (follow->len > 1)
            insts_len +=
                sizeof(byte) + sizeof(len_t) + follow->len * sizeof(offset_t);
        /* allocate space for each list of actions */
        FOREACH(pp, follow->sentinel) {
            insts_len += emit_transition(pp, NULL, NULL, NULL);
            /* add target state for processing if not visited */
            if (!visited[pp->pos]) {
                stc_vec_push(states, pp->pos);
                visited[pp->pos] = TRUE;
            }
        }
    }
    free(visited);
    stc_vec_free(states);

    /* save where `match` instruction will go */
    pc_map[GAMMA_POS] = insts_len;
    return insts_len + 1;
}

static size_t
count(const Regex *re, len_t *aux_len, len_t *ncaptures, len_t *ncounters)
{
    size_t npos = 0;

    switch (re->type) {
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case MEMOISE: /* fallthrough */
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

        case NREGEXTYPES: assert(0 && "unreachable");
    }

    return npos;
}

static byte *emit(const Rfa *rfa, byte *pc, Program *prog, len_t *pc_map)
{
    byte        *visited = malloc(rfa->npositions * sizeof(byte));
    size_t      *states  = NULL;
    size_t       pos;
    PosPairList *follow;
    PosPair     *pp;
    const Regex *re;
    offset_t     x;

    /* initialise processing */
    memset(visited, 0, rfa->npositions * sizeof(byte));
    stc_vec_init(states, rfa->npositions);
    stc_vec_push(states, START_POS);
    visited[START_POS] = TRUE;

    /* process each state in states (possibly adding more states) */
    while (stc_vec_len_unsafe(states) > 0) {
        pos = stc_vec_pop(states);
        if (pos != START_POS) {
            switch ((re = rfa->positions[pos])->type) {
                case LITERAL:
                    *pc++ = CHAR;
                    MEMWRITE(pc, const char *, re->ch);
                    break;
                case CC:
                    *pc++ = PRED;
                    MEMWRITE(pc, len_t, re->cc_len);
                    memcpy(prog->aux + stc_vec_len_unsafe(prog->aux),
                           re->intervals, re->cc_len * sizeof(Interval));
                    MEMWRITE(pc, len_t, stc_vec_len_unsafe(prog->aux));
                    stc_vec_len_unsafe(prog->aux) +=
                        re->cc_len * sizeof(Interval);
                    break;

                case DOLLAR: *pc++ = END; break;
                case CARET: *pc++ = BEGIN; break;
                case MEMOISE: *pc++ = MEMO; break;
                default: assert(0 && "unreachable");
            }
        }

        follow = rfa->follow[pos];
        /* write twsitch instruction with correct offsets */
        /* TODO: optimise for actionless transitions */
        if (follow->len > 1) {
            *pc++ = TSWITCH;
            MEMWRITE(pc, len_t, follow->len);
            x = follow->len * sizeof(offset_t);
            FOREACH(pp, follow->sentinel) {
                MEMWRITE(pc, offset_t, x - sizeof(offset_t));
                /* XXX: may be more involved with counters later */
                x += emit_transition(pp, NULL, prog, pc_map);
                x -= sizeof(offset_t); /* since we are 1 offset closer to end */
            }
        }
        /* write the VM instructions for each set of actions */
        FOREACH(pp, follow->sentinel) {
            emit_transition(pp, &pc, prog, pc_map);
            /* add target state for processing if not visited */
            if (!visited[pp->pos]) {
                stc_vec_push(states, pp->pos);
                visited[pp->pos] = TRUE;
            }
        }
    }
    free(visited);
    stc_vec_free(states);

    /* write `match` instruction */
    *pc = MATCH;
    return pc;
}

static len_t
emit_transition(PosPair *pos_pair, byte **pc, Program *prog, len_t *pc_map)
{
    Action *action;
    len_t   insts_len = 0;

    /* XXX: may be more involved with counters later */
    FOREACH(action, pos_pair->action_sentinel) {
        switch (action->re->type) {
            case CAPTURE:
                insts_len += sizeof(byte) + sizeof(len_t);
                if (pc) {
                    *(*pc)++ = SAVE;
                    MEMWRITE(*pc, len_t, action->re->capture_idx);
                }
                break;
            default: assert(0 && "unreachable");
        }
    }
    /* TODO: optimise out jmp if possible */
    insts_len += sizeof(byte) + sizeof(offset_t);
    if (pc) {
        *(*pc)++ = JMP;
        MEMWRITE(*pc, offset_t,
                 pc_map[pos_pair->pos] -
                     (*pc + sizeof(offset_t) - prog->insts));
    }

    return insts_len;
}

/* --- Debug functions ------------------------------------------------------ */

#if defined(DEBUG) || defined(DEBUG_GLUSHKOV)
static void ppl_print(PosPairList *self, FILE *stream)
{
    PosPair *pp;
    Action  *act;

    FOREACH(pp, self->sentinel) {
        fprintf(stream, "%lu (", pp->pos);
        FOREACH(act, pp->action_sentinel) {
            if (act != pp->action_sentinel->next) fprintf(stream, ", ");
            switch (act->re->type) {
                case CARET: fprintf(stream, "^"); break;
                case MEMOISE: fprintf(stream, "#"); break;
                case DOLLAR: fprintf(stream, "$"); break;
                case CAPTURE:
                    fprintf(stream, "%c_%d",
                            act->re->capture_idx % 2 == 0 ? '[' : ']',
                            act->re->capture_idx / 2);
                    break;
                default:
                    fprintf(stream, "action type = %d\n", act->re->type);
                    assert(0 && "unreachable");
            }
        }
        fprintf(stream, ") ");
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
