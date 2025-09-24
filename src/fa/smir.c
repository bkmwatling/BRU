#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <stc/fatp/vec.h>

#include <bru/fa/smir.h>
#include <bru/utils.h>

/* --- Preprocessor directives ---------------------------------------------- */

#define trans_id_from_parts(sid, idx) ((((bru_trans_id) sid) << 32) | idx)

#define trans_id_sid(tid) ((bru_state_id) (tid >> 32))

#define trans_id_idx(tid) ((uint32_t) (tid & 0xffffffff))

#define trans_init(trans)                        \
    do {                                         \
        BRU_DLL_INIT(trans);                     \
        BRU_DLL_INIT((trans)->actions_sentinel); \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_trans BruTrans;

typedef struct {
    BruActionList *actions_sentinel;
    size_t         nactions;
    BruTrans      *out_transitions_sentinel;
    size_t         nout;
    void          *pre_meta;
    void          *post_meta;
} BruState;

struct bru_trans {
    BruActionList *actions_sentinel;
    size_t         nactions;
    bru_state_id   src;
    bru_state_id   dst;
    BruTrans      *prev;
    BruTrans      *next;
};

struct bru_action_list {
    const BruAction *act;
    BruActionList   *prev;
    BruActionList   *next;
};

struct bru_action_list_iter {
    const BruActionList *sentinel;
    BruActionList       *current;
};

struct bru_state_machine {
    const char      *regex;
    StcVec(BruState) states;
    BruTrans        *initial_functions_sentinel;
    size_t           ninits;
};

/* --- Helper functions ----------------------------------------------------- */

static void action_list_free(BruActionList *self)
{
    if (!self) return;

    bru_smir_action_free(self->act);
    free(self);
}

static void trans_free(BruTrans *self)
{
    if (!self) return;

    if (self->actions_sentinel)
        bru_smir_action_list_free(self->actions_sentinel);
    free(self);
}

static void state_free(BruState *self)
{
    BruTrans *elem, *next;

    if (self->actions_sentinel)
        bru_smir_action_list_free(self->actions_sentinel);
    if (self->out_transitions_sentinel)
        BRU_DLL_FREE(self->out_transitions_sentinel, trans_free, elem, next);
}

/* --- API function definitions --------------------------------------------- */

BruStateMachine *bru_smir_default(const char *regex)
{
    BruStateMachine *sm = malloc(sizeof(*sm));

    sm->regex  = regex;
    sm->ninits = 0;
    stc_vec_default_init(&sm->states);
    BRU_DLL_INIT(sm->initial_functions_sentinel);

    return sm;
}

BruStateMachine *bru_smir_new(const char *regex, uint32_t nstates)
{
    BruStateMachine *sm = malloc(sizeof(*sm));

    sm->regex  = regex;
    sm->ninits = 0;
    stc_vec_init(&sm->states, nstates);
    while (nstates--) bru_smir_add_state(sm);
    BRU_DLL_INIT(sm->initial_functions_sentinel);

    return sm;
}

void bru_smir_free(BruStateMachine *self)
{
    BruTrans *elem, *next;
    size_t    nstates;

    if (!self) return;

    if (self->states) {
        nstates = stc_vec_len(self->states);
        while (nstates) state_free(&self->states[--nstates]);
        stc_vec_free(self->states);
    }

    if (self->initial_functions_sentinel)
        BRU_DLL_FREE(self->initial_functions_sentinel, trans_free, elem, next);

    free(self);
}

bru_state_id bru_smir_add_state(BruStateMachine *self)
{
    BruState state = { 0 };

    BRU_DLL_INIT(state.actions_sentinel);
    BRU_DLL_INIT(state.out_transitions_sentinel);
    stc_vec_push_back(&self->states, state);

    return stc_vec_len(self->states);
}

size_t bru_smir_get_num_states(BruStateMachine *self)
{
    return stc_vec_len(self->states);
}

const char *bru_smir_get_regex(BruStateMachine *self) { return self->regex; }

bru_trans_id bru_smir_set_initial(BruStateMachine *self, bru_state_id sid)
{
    BruTrans *transition;

    trans_init(transition);
    transition->dst = sid;
    BRU_DLL_PUSH_BACK(self->initial_functions_sentinel, transition);

    return trans_id_from_parts(BRU_NULL_STATE, self->ninits++);
}

bru_trans_id *bru_smir_get_initial(BruStateMachine *self, size_t *n)
{
    return bru_smir_get_out_transitions(self, BRU_NULL_STATE, n);
}

bru_trans_id bru_smir_set_final(BruStateMachine *self, bru_state_id sid)
{
    return bru_smir_add_transition(self, sid);
}

bru_trans_id bru_smir_add_transition(BruStateMachine *self, bru_state_id sid)
{
    BruTrans *transition, *transitions;
    size_t   *n;

    trans_init(transition);
    transition->src = sid;
    if (sid) {
        transitions = self->states[sid - 1].out_transitions_sentinel;
        n           = &self->states[sid - 1].nout;
    } else {
        transitions = self->initial_functions_sentinel;
        n           = &self->ninits;
    }
    BRU_DLL_PUSH_BACK(transitions, transition);

    return trans_id_from_parts(sid, (*n)++);
}

void bru_smir_remove_transition(BruStateMachine *self, bru_trans_id tid)
{
    bru_state_id sid = trans_id_sid(tid);
    uint32_t     idx = trans_id_idx(tid);
    BruTrans    *transitions, *transition;

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;

    BRU_DLL_GET(transitions, idx, transition);

    transition->prev->next = transition->next;
    transition->next->prev = transition->prev;
    trans_free(transition);
}

bru_trans_id *
bru_smir_get_out_transitions(BruStateMachine *self, bru_state_id sid, size_t *n)
{
    bru_trans_id *tids;
    size_t        i, m;

    m    = sid ? self->states[sid - 1].nout : self->ninits;
    tids = malloc(m * sizeof(*tids));
    for (i = 0; i < m; i++) tids[i] = trans_id_from_parts(sid, i);
    if (n) *n = m;

    return tids;
}

size_t bru_smir_state_get_num_actions(BruStateMachine *self, bru_state_id sid)
{
    return sid ? self->states[sid - 1].nactions : 0;
}

const BruActionList *bru_smir_state_get_actions(BruStateMachine *self,
                                                bru_state_id     sid)
{
    return sid ? self->states[sid - 1].actions_sentinel : NULL;
}

void bru_smir_state_append_action(BruStateMachine *self,
                                  bru_state_id     sid,
                                  const BruAction *act)
{
    BruActionList *al;
    if (sid) {
        al      = malloc(sizeof(*al));
        al->act = act;
        BRU_DLL_PUSH_BACK(self->states[sid - 1].actions_sentinel, al);
        self->states[sid - 1].nactions++;
    }
}

void bru_smir_state_prepend_action(BruStateMachine *self,
                                   bru_state_id     sid,
                                   const BruAction *act)
{
    BruActionList *al;
    if (sid) {
        al      = malloc(sizeof(*al));
        al->act = act;
        BRU_DLL_PUSH_FRONT(self->states[sid - 1].actions_sentinel, al);
        self->states[sid - 1].nactions++;
    }
}

void bru_smir_state_set_actions(BruStateMachine *self,
                                bru_state_id     sid,
                                BruActionList   *acts)
{
    BruState *state;

    if (!(sid && acts)) return;

    state = &self->states[sid - 1];
    bru_smir_action_list_free(state->actions_sentinel);
    state->actions_sentinel = acts;

    // recalculate the number of actions
    for (state->nactions = 0, acts                                = acts->next;
         acts != state->actions_sentinel; state->nactions++, acts = acts->next);
}

BruActionList *bru_smir_state_clone_actions(BruStateMachine *self,
                                            bru_state_id     sid)
{
    return bru_smir_action_list_clone(bru_smir_state_get_actions(self, sid));
}

bru_state_id bru_smir_get_src(BruStateMachine *self, bru_trans_id tid)
{
    (void) self;
    return trans_id_sid(tid);
}

bru_state_id bru_smir_get_dst(BruStateMachine *self, bru_trans_id tid)
{
    BruTrans    *transition, *transitions;
    bru_state_id sid = trans_id_sid(tid);
    uint32_t     idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    BRU_DLL_GET(transitions, idx, transition);

    return transition->dst;
}

void bru_smir_set_dst(BruStateMachine *self, bru_trans_id tid, bru_state_id dst)
{
    BruTrans    *transition, *transitions;
    bru_state_id sid = trans_id_sid(tid);
    uint32_t     idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    BRU_DLL_GET(transitions, idx, transition);
    transition->dst = dst;
}

size_t bru_smir_trans_get_num_actions(BruStateMachine *self, bru_trans_id tid)
{
    BruTrans    *transition, *transitions;
    bru_state_id sid = trans_id_sid(tid);
    uint32_t     idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    BRU_DLL_GET(transitions, idx, transition);

    return transition->nactions;
}

const BruActionList *bru_smir_trans_get_actions(BruStateMachine *self,
                                                bru_trans_id     tid)
{
    BruTrans    *transition, *transitions;
    bru_state_id sid = trans_id_sid(tid);
    uint32_t     idx = trans_id_idx(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    BRU_DLL_GET(transitions, idx, transition);

    return transition->actions_sentinel;
}

void bru_smir_trans_append_action(BruStateMachine *self,
                                  bru_trans_id     tid,
                                  const BruAction *act)
{
    BruTrans      *transition, *transitions;
    BruActionList *al  = malloc(sizeof(*al));
    bru_state_id   sid = trans_id_sid(tid);
    uint32_t       idx = trans_id_idx(tid);

    al->act     = act;
    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    BRU_DLL_GET(transitions, idx, transition);
    BRU_DLL_PUSH_BACK(transition->actions_sentinel, al);
    transition->nactions++;
}

void bru_smir_trans_prepend_action(BruStateMachine *self,
                                   bru_trans_id     tid,
                                   const BruAction *act)
{
    BruTrans      *transition, *transitions;
    BruActionList *al  = malloc(sizeof(*al));
    bru_state_id   sid = trans_id_sid(tid);
    uint32_t       idx = trans_id_idx(tid);

    al->act     = act;
    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    BRU_DLL_GET(transitions, idx, transition);
    BRU_DLL_PUSH_FRONT(transition->actions_sentinel, al);
    transition->nactions++;
}

void bru_smir_trans_set_actions(BruStateMachine *self,
                                bru_trans_id     tid,
                                BruActionList   *acts)
{
    BruTrans    *transition, *transitions;
    bru_state_id sid = trans_id_sid(tid);
    uint32_t     idx = trans_id_idx(tid);

    if (!acts) return;

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    BRU_DLL_GET(transitions, idx, transition);
    bru_smir_action_list_free(transition->actions_sentinel);
    transition->actions_sentinel = acts;

    // recalculate the number of actions
    for (transition->nactions = 0, acts = acts->next;
         acts != transition->actions_sentinel;
         transition->nactions++, acts = acts->next);
}

BruActionList *bru_smir_trans_clone_actions(BruStateMachine *self,
                                            bru_trans_id     tid)
{
    return bru_smir_action_list_clone(bru_smir_trans_get_actions(self, tid));
}

void bru_smir_print(BruStateMachine *self, FILE *stream)
{
    bru_state_id  sid;
    bru_trans_id *out;
    size_t        n, i;

    if (self->ninits) {
        fprintf(stream, "Initialisation:\n");
        out = bru_smir_get_out_transitions(self, BRU_INITIAL_STATE_ID, &n);
        for (i = 0; i < n; i++) {
            fprintf(stream, "  %u: ", bru_smir_get_dst(self, out[i]));
            bru_smir_action_list_print(bru_smir_trans_get_actions(self, out[i]),
                                       stream);
            fprintf(stream, "\n");
        }
        free(out);
    }

    for (sid = 1; sid <= bru_smir_get_num_states(self); sid++) {
        fprintf(stream, "State(%u): ", sid);
        bru_smir_action_list_print(bru_smir_state_get_actions(self, sid),
                                   stream);
        fprintf(stream, "\n");

        out = bru_smir_get_out_transitions(self, sid, &n);
        for (i = 0; i < n; i++) {
            fprintf(stream, "  %u: ", bru_smir_get_dst(self, out[i]));
            bru_smir_action_list_print(bru_smir_trans_get_actions(self, out[i]),
                                       stream);
            fprintf(stream, "\n");
        }
        free(out);
    }
}

/* --- Action and ActionList functions -------------------------------------- */

const BruAction *bru_smir_action_zwa(BruActionType type)
{
    BruAction *act = malloc(sizeof(*act));

    assert(type == BRU_ACT_BEGIN || type == BRU_ACT_END);
    act->type = type;

    return act;
}

const BruAction *bru_smir_action_char(const char *ch)
{
    BruAction *act = malloc(sizeof(*act));

    act->type = BRU_ACT_CHAR;
    act->ch   = ch;

    return act;
}

const BruAction *bru_smir_action_predicate(const BruIntervals *pred)
{
    BruAction *act = malloc(sizeof(*act));

    act->type = BRU_ACT_PRED;
    act->pred = pred;

    return act;
}

const BruAction *bru_smir_action_num(BruActionType type, size_t k)
{
    BruAction *act = malloc(sizeof(*act));

    act->type = type;
    if (type == BRU_ACT_WRITE)
        act->c = (char) k;
    else
        act->k = k;

    return act;
}

const BruAction *bru_smir_action_set(size_t k, bru_cntr_t val)
{
    BruAction *act = malloc(sizeof(*act));

    act->type = BRU_ACT_SET;
    act->k    = k;
    act->val  = val;

    return act;
}

const BruAction *bru_smir_action_cmp(size_t k, bru_cntr_t val, BruOrd ord)
{
    BruAction *act = malloc(sizeof(*act));

    act->type = BRU_ACT_CMP;
    act->k    = k;
    act->val  = val;
    act->ord  = ord;

    return act;
}

const BruAction *bru_smir_action_clone(const BruAction *self)
{
    const BruAction *clone;

    if (!self) return NULL;

    switch (self->type) {
        case BRU_ACT_BEGIN: /* fallthrough */
        case BRU_ACT_END: clone = bru_smir_action_zwa(self->type); break;

        case BRU_ACT_CHAR: clone = bru_smir_action_char(self->ch); break;
        case BRU_ACT_PRED:
            clone = bru_smir_action_predicate(bru_intervals_clone(self->pred));
            break;

        case BRU_ACT_SAVE:    /* fallthrough */
        case BRU_ACT_BACKREF: /* fallthrough */
        case BRU_ACT_INC:     /* fallthrough */
        case BRU_ACT_EPSSET:  /* fallthrough */
        case BRU_ACT_EPSCHK:  /* fallthrough */
        case BRU_ACT_MEMOSET: /* fallthrough */
        case BRU_ACT_MEMOCHK: /* fallthrough */
        case BRU_ACT_WRITE:
            clone = bru_smir_action_num(self->type, self->k);
            break;

        case BRU_ACT_SET:
            clone = bru_smir_action_set(self->k, self->val);
            break;

        case BRU_ACT_CMP:
            clone = bru_smir_action_cmp(self->k, self->val, self->ord);
            break;

        case BRU_ACT_NACTIONS: assert(false && "unreachable"); break;
    }

    return clone;
}

void bru_smir_action_free(const BruAction *self)
{
    if (!self) return;

    if (self->type == BRU_ACT_PRED)
        bru_intervals_free((BruIntervals *) self->pred);
    free((BruAction *) self);
}

bool bru_smir_action_equal(const BruAction *a1, const BruAction *a2)
{
    if (a1->type != a2->type) return false;
    switch (a1->type) {
        case BRU_ACT_BEGIN: /* fallthrough */
        case BRU_ACT_END: return true;

        case BRU_ACT_CHAR: return stc_utf8_cmp(a1->ch, a2->ch) == 0;
        case BRU_ACT_PRED:
            assert(false && "TODO: equality of predicates");
            break;

        case BRU_ACT_SAVE:    /* fallthrough */
        case BRU_ACT_BACKREF: /* fallthrough */
        case BRU_ACT_INC:     /* fallthrough */
        case BRU_ACT_MEMOSET: /* fallthrough */
        case BRU_ACT_MEMOCHK: /* fallthrough */
        case BRU_ACT_EPSSET:  /* fallthrough */
        case BRU_ACT_EPSCHK: return a1->k == a2->k;

        case BRU_ACT_SET: return a1->k == a2->k && a1->val == a2->val;

        case BRU_ACT_CMP:
            return a1->k == a2->k && a1->val == a2->val && a1->ord == a2->ord;

        case BRU_ACT_WRITE: return a1->c == a2->c;

        case BRU_ACT_NACTIONS: /* fallthrough */
        default: assert(false && "unreachable"); break;
    }
}

BruActionType bru_smir_action_type(const BruAction *self) { return self->type; }

size_t bru_smir_action_get_num(const BruAction *self)
{
    return BRU_ACT_SAVE <= self->type && self->type <= BRU_ACT_MEMOCHK ? self->k
                                                                       : 0;
}

void bru_smir_action_print(const BruAction *self, FILE *stream)
{
    char *s;

    switch (self->type) {
        case BRU_ACT_BEGIN: fputs("begin", stream); break;
        case BRU_ACT_END: fputs("end", stream); break;

        case BRU_ACT_CHAR:
            fprintf(stream, "char %.*s", stc_utf8_nbytes(self->ch), self->ch);
            break;
        case BRU_ACT_PRED:
            s = bru_intervals_to_str(self->pred);
            fprintf(stream, "pred %s", s);
            free(s);
            break;

        case BRU_ACT_SAVE: fprintf(stream, "save %zu", self->k); break;
        case BRU_ACT_BACKREF: fprintf(stream, "backref %zu", self->k); break;
        case BRU_ACT_INC: fprintf(stream, "inc %zu", self->k); break;
        case BRU_ACT_SET:
            fprintf(stream, "set %zu, " BRU_CNTR_FMT, self->k, self->val);
            break;
        case BRU_ACT_CMP:
            switch (self->ord) {
                case BRU_LT: fputs("cmplt ", stream); break;
                case BRU_LE: fputs("cmple ", stream); break;
                case BRU_EQ: fputs("cmpeq ", stream); break;
                case BRU_NE: fputs("cmpne ", stream); break;
                case BRU_GE: fputs("cmpge ", stream); break;
                case BRU_GT: fputs("cmpgt ", stream); break;
            }
            fprintf(stream, "%zu, " BRU_CNTR_FMT, self->k, self->val);
            break;

        case BRU_ACT_EPSSET: fprintf(stream, "epsset %zu", self->k); break;
        case BRU_ACT_EPSCHK: fprintf(stream, "epschk %zu", self->k); break;
        case BRU_ACT_MEMOSET: fprintf(stream, "memoset %zu", self->k); break;
        case BRU_ACT_MEMOCHK: fprintf(stream, "memochk %zu", self->k); break;
        case BRU_ACT_WRITE:
            fprintf(stream,
                    self->c == '0' ? "write0"
                                   : (self->c == '1' ? "write1" : "write %c"),
                    self->c);
            break;

        case BRU_ACT_NACTIONS: assert(false && "unreachable"); break;
    }
}

BruActionList *bru_smir_action_list_new(void)
{
    BruActionList *action_list;

    BRU_DLL_INIT(action_list);

    return action_list;
}

BruActionList *bru_smir_action_list_clone(const BruActionList *self)
{
    BruActionList *clone;

    BRU_DLL_INIT(clone);
    bru_smir_action_list_copy(self, clone);

    return clone;
}

void bru_smir_action_list_copy(const BruActionList *self, BruActionList *dst)
{
    BruActionList       *al;
    const BruActionList *tmp;

    for (tmp = self->next; tmp != self; tmp = tmp->next) {
        al      = malloc(sizeof(*al));
        al->act = bru_smir_action_clone(tmp->act);
        BRU_DLL_PUSH_BACK(dst, al);
    }
}

void bru_smir_action_list_clear(BruActionList *self)
{
    BruActionList *next, *elem;

    for (elem = self->next; elem != self; elem = next) {
        next = elem->next;
        action_list_free(elem);
    }
    self->next = self;
    self->prev = self;
}

void bru_smir_action_list_free(BruActionList *self)
{
    BruActionList *elem, *next;
    BRU_DLL_FREE(self, action_list_free, elem, next);
}

size_t bru_smir_action_list_len(const BruActionList *self)
{
    size_t               len;
    const BruActionList *iter;

    if (!self) return 0;

    for (len = 0, iter = self->next; iter != self; len++, iter = iter->next);

    return len;
}

void bru_smir_action_list_push_back(BruActionList *self, const BruAction *act)
{
    BruActionList *al = malloc(sizeof(*al));

    al->act = act;
    BRU_DLL_PUSH_BACK(self, al);
}

void bru_smir_action_list_push_front(BruActionList *self, const BruAction *act)
{
    BruActionList *al = malloc(sizeof(*al));

    al->act = act;
    BRU_DLL_PUSH_FRONT(self, al);
}

void bru_smir_action_list_append(BruActionList *self, BruActionList *acts)
{
    if (!acts || acts->next == acts) return;

    self->prev->next = acts->next;
    acts->next->prev = self->prev;
    acts->prev->next = self;
    self->prev       = acts->prev;
    acts->prev = acts->next = acts;
}

void bru_smir_action_list_prepend(BruActionList *self, BruActionList *acts)
{
    if (!acts || acts->next == acts) return;

    self->next->prev = acts->prev;
    acts->prev->next = self->next;
    acts->next->prev = self;
    self->next       = acts->next;
    acts->next = acts->prev = acts;
}

BruActionListIter *bru_smir_action_list_iter(const BruActionList *self)
{
    BruActionListIter *iter = malloc(sizeof(*iter));

    iter->sentinel = self;
    iter->current  = NULL;

    return iter;
}

const BruAction *bru_smir_action_list_iter_next(BruActionListIter *self)
{
    BruActionList *al = self->current;

    if (self->current == self->sentinel) return NULL;
    self->current = self->current ? self->current->next : self->sentinel->next;
    if (al && al->act == NULL) {
        // marked for removal in bru_smir_action_list_iterator_remove
        free(al);
    }
    if (self->current == self->sentinel) return NULL;

    return self->current->act;
}

const BruAction *bru_smir_action_list_iter_prev(BruActionListIter *self)
{
    BruActionList *al = self->current;

    if (self->current == self->sentinel) return NULL;
    self->current = self->current ? self->current->prev : self->sentinel->prev;
    if (al && al->act == NULL) {
        // marked for removal in bru_smir_action_list_iterator_remove
        free(al);
    }
    if (self->current == self->sentinel) return NULL;

    return self->current->act;
}

void bru_smir_action_list_iter_remove(BruActionListIter *self)
{
    BruActionList *al;

    if (!self->current || self->current->act == NULL ||
        self->current == self->sentinel)
        return;

    al             = self->current;
    al->prev->next = al->next;
    al->next->prev = al->prev;

    bru_smir_action_free(al->act);
    al->act = NULL;
}

void bru_smir_action_list_iter_free(BruActionListIter *self)
{
    if (self->current != self->sentinel && self->current &&
        self->current->act == NULL)
        // marked for removal in bru_smir_action_list_iterator_remove
        free(self->current);

    free(self);
}

void bru_smir_action_list_print(const BruActionList *self, FILE *stream)
{
    const BruActionList *al;

    for (al = self->next; al != self; al = al->next) {
        bru_smir_action_print(al->act, stream);
        if (al->next != self) fprintf(stream, ", ");
    }

    if (self->next == self) fprintf(stream, "-|");
}

/* --- Extendable API function definitions ---------------------------------- */

void *bru_smir_set_pre_meta(BruStateMachine *self, bru_state_id sid, void *meta)
{
    void *old_meta;

    if (!sid) return NULL;

    old_meta                       = self->states[sid - 1].pre_meta;
    self->states[sid - 1].pre_meta = meta;

    return old_meta;
}

void *bru_smir_get_pre_meta(BruStateMachine *self, bru_state_id sid)
{
    return sid ? self->states[sid - 1].pre_meta : NULL;
}

void *
bru_smir_set_post_meta(BruStateMachine *self, bru_state_id sid, void *meta)
{
    void *old_meta;

    if (!sid) return NULL;

    old_meta                        = self->states[sid - 1].post_meta;
    self->states[sid - 1].post_meta = meta;

    return old_meta;
}

void *bru_smir_get_post_meta(BruStateMachine *self, bru_state_id sid)
{
    return sid ? self->states[sid - 1].post_meta : NULL;
}

void bru_smir_reorder_states(BruStateMachine *self, bru_state_id *sid_ordering)
{
    StcVec(BruState) states;
    bru_trans_id    *out;
    bru_state_id     sid, dst;
    size_t           i, n, nstates;

    if (!sid_ordering) return;

    // update destinations of transitions
    nstates = bru_smir_get_num_states(self);
    for (sid = 0; sid <= nstates; sid++) {
        out = bru_smir_get_out_transitions(self, sid, &n);
        for (i = 0; i < n; i++) {
            dst = bru_smir_get_dst(self, out[i]);
            bru_smir_set_dst(self, out[i], dst ? sid_ordering[dst - 1] : 0);
        }
        free(out);
    }

    // reorder the states in the states array
    stc_vec_init(&states, nstates);
    if (nstates) stc_vec_len(states) = nstates;
    for (sid = 1; sid <= nstates; sid++)
        states[sid_ordering[sid - 1] - 1] = self->states[sid - 1];

    stc_vec_free(self->states);
    self->states = states;
}
