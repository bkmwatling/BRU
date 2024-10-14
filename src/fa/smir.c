#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../stc/fatp/vec.h"

#include "../utils.h"
#include "smir.h"

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

struct bru_action {
    BruActionType type;

    union {
        const char         *ch;   /**< type = ACT_CHAR                        */
        const BruIntervals *pred; /**< type = ACT_PRED                        */
        size_t k; /**< type = ACT_MEMO | ACT_SAVE | ACT_EPSCHK | ACT_EPSSET   */
        char   c; /**< type = ACT_WRITE                                       */
    };
};

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

struct bru_action_list_iterator {
    const BruActionList *sentinel;
    BruActionList       *current;
};

struct bru_state_machine {
    const char *regex;
    BruState   *states;
    BruTrans   *initial_functions_sentinel;
    size_t      ninits;
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
    stc_vec_default_init(sm->states);
    BRU_DLL_INIT(sm->initial_functions_sentinel);

    return sm;
}

BruStateMachine *bru_smir_new(const char *regex, uint32_t nstates)
{
    BruStateMachine *sm = malloc(sizeof(*sm));

    sm->regex  = regex;
    sm->ninits = 0;
    stc_vec_init(sm->states, nstates);
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
        nstates = stc_vec_len_unsafe(self->states);
        while (nstates) state_free(&self->states[--nstates]);
        stc_vec_free(self->states);
    }

    if (self->initial_functions_sentinel)
        BRU_DLL_FREE(self->initial_functions_sentinel, trans_free, elem, next);

    free(self);
}

BruProgram *bru_smir_compile(BruStateMachine *self)
{
    return bru_smir_compile_with_meta(self, NULL, NULL);
}

bru_state_id bru_smir_add_state(BruStateMachine *self)
{
    BruState state = { 0 };

    BRU_DLL_INIT(state.actions_sentinel);
    BRU_DLL_INIT(state.out_transitions_sentinel);
    stc_vec_push_back(self->states, state);

    return stc_vec_len_unsafe(self->states);
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

        case BRU_ACT_WRITE:  /* fallthrough */
        case BRU_ACT_MEMO:   /* fallthrough */
        case BRU_ACT_SAVE:   /* fallthrough */
        case BRU_ACT_EPSCHK: /* fallthrough */
        case BRU_ACT_EPSSET:
            clone = bru_smir_action_num(self->type, self->k);
            break;
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

BruActionType bru_smir_action_type(const BruAction *self) { return self->type; }

size_t bru_smir_action_get_num(const BruAction *self)
{
    return BRU_ACT_MEMO <= self->type && self->type <= BRU_ACT_EPSSET ? self->k
                                                                      : 0;
}

void bru_smir_action_print(const BruAction *self, FILE *stream)
{
    char *s;

    switch (self->type) {
        case BRU_ACT_BEGIN: fprintf(stream, "begin"); break;
        case BRU_ACT_END: fprintf(stream, "end"); break;
        case BRU_ACT_CHAR:
            fprintf(stream, "char %.*s", stc_utf8_nbytes(self->ch), self->ch);
            break;
        case BRU_ACT_PRED:
            s = bru_intervals_to_str(self->pred);
            fprintf(stream, "pred %s", s);
            free(s);
            break;
        case BRU_ACT_WRITE:
            fprintf(stream,
                    self->c == '0'   ? "write0"
                    : self->c == '1' ? "write1"
                                     : "write %c",
                    self->c);
            break;
        case BRU_ACT_MEMO: fprintf(stream, "memo %zu", self->k); break;
        case BRU_ACT_SAVE: fprintf(stream, "save %zu", self->k); break;
        case BRU_ACT_EPSCHK: fprintf(stream, "epschk %zu", self->k); break;
        case BRU_ACT_EPSSET: fprintf(stream, "epsset %zu", self->k); break;
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
    bru_smir_action_list_clone_into(self, clone);

    return clone;
}

void bru_smir_action_list_clone_into(const BruActionList *self,
                                     BruActionList       *clone)
{
    BruActionList       *al;
    const BruActionList *tmp;

    for (tmp = self->next; tmp != self; tmp = tmp->next) {
        al      = malloc(sizeof(*al));
        al->act = bru_smir_action_clone(tmp->act);
        BRU_DLL_PUSH_BACK(clone, al);
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

BruActionListIterator *bru_smir_action_list_iter(const BruActionList *self)
{
    BruActionListIterator *iter = malloc(sizeof(*iter));

    iter->sentinel = self;
    iter->current  = NULL;

    return iter;
}

const BruAction *bru_smir_action_list_iterator_next(BruActionListIterator *self)
{
    if (self->current == self->sentinel) return NULL;
    self->current = self->current ? self->current->next : self->sentinel->next;
    if (self->current == self->sentinel) return NULL;

    return self->current->act;
}

const BruAction *bru_smir_action_list_iterator_prev(BruActionListIterator *self)
{
    if (self->current == self->sentinel) return NULL;
    self->current = self->current ? self->current->prev : self->sentinel->prev;
    if (self->current == self->sentinel) return NULL;

    return self->current->act;
}

void bru_smir_action_list_iterator_remove(BruActionListIterator *self)
{
    BruActionList *al;

    if (!self->current || self->current == self->sentinel) return;

    al             = self->current;
    al->prev->next = al->next;
    al->next->prev = al->prev;
    self->current  = al->prev == self->sentinel ? NULL : al->prev;

    al->prev = al->next = NULL;
    bru_smir_action_free(al->act);
    free(al);
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
    BruState     *states;
    bru_trans_id *out;
    bru_state_id  sid, dst;
    size_t        i, n, nstates;

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
    stc_vec_init(states, nstates);
    if (nstates) stc_vec_len_unsafe(states) = nstates;
    for (sid = 1; sid <= nstates; sid++)
        states[sid_ordering[sid - 1] - 1] = self->states[sid - 1];

    stc_vec_free(self->states);
    self->states = states;
}

/* --- SMIR compilation ----------------------------------------------------- */

#define RESERVE(bytes, n)               \
    do {                                \
        stc_vec_reserve(bytes, n);      \
        stc_vec_len_unsafe(bytes) += n; \
    } while (0)

#define PC(insts) ((insts) + stc_vec_len_unsafe(insts))

#define SET_OFFSET(insts, offset_idx, idx)         \
    *((bru_offset_t *) ((insts) + (offset_idx))) = \
        (bru_offset_t) (idx) - ((offset_idx) + sizeof(bru_offset_t))

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    bru_offset_t entry; /**< where the state is compiled in the program       */
    bru_offset_t exit;  /**< the to-be-filled outgoing transition offsets     */
    size_t transitions; /**< where the transitions start                      */
} BruStateBlock;

typedef struct {
    bru_regex_id rid; /**< the regex identifier for the mapping               */
    bru_len_t    idx; /**< the index the regex identifier is mapped to        */
} BruRidToIdx;

typedef struct {
    BruRidToIdx *thread_map;           /**< stc_vec for thread mapping        */
    BruRidToIdx *memoisation_map;      /**< stc_vec for memoisation mapping   */
    bru_len_t    next_thread_idx;      /**< next index for thread map         */
    bru_len_t    next_memoisation_idx; /**< next index for memoisation map    */

    // TODO: counter memory
} BruMemoryMaps; // map RIDs to memory indices

/* --- Helper function definitions ------------------------------------------ */

static size_t count_bytes_actions(const BruActionList *acts)
{
    BruActionList *n;
    size_t         size;

    if (!acts) return 0;

    for (n = acts->next, size = 0; n != acts; n = n->next) {
        switch (n->act->type) {
            case BRU_ACT_BEGIN: size++; break;
            case BRU_ACT_END: size++; break;

            case BRU_ACT_CHAR:
                size++;
                size += sizeof(const char *);
                break;

            case BRU_ACT_PRED:
                size++;
                size += sizeof(bru_len_t);
                break;

            case BRU_ACT_WRITE:
                size++;
                if (n->act->c != '0' && n->act->c != '1') size++;
                break;

            case BRU_ACT_MEMO:
                size++;
                size += sizeof(bru_len_t);
                break;

            case BRU_ACT_EPSCHK:
                size++;
                size += sizeof(bru_len_t);
                break;

            case BRU_ACT_SAVE:
                size++;
                size += sizeof(bru_len_t);
                break;

            case BRU_ACT_EPSSET:
                size++;
                size += sizeof(bru_len_t);
                break;
        }
    }

    return size;
}

static bru_byte_t *compile_actions(bru_byte_t          *pc,
                                   BruProgram          *prog,
                                   const BruActionList *acts,
                                   BruMemoryMaps       *mmaps)
{
#define GET_IDX(mmap, next_idx, idx_inc, uid)                          \
    do {                                                               \
        for (idx = 0, len = stc_vec_len_unsafe(mmap);                  \
             idx < len && (mmap)[idx].rid != (uid); idx++);            \
        if (idx == len) {                                              \
            idx         = (next_idx);                                  \
            (next_idx) += (idx_inc);                                   \
            stc_vec_push_back(mmap, ((BruRidToIdx) { (uid), (idx) })); \
        } else {                                                       \
            idx = (mmap)[idx].idx;                                     \
        }                                                              \
    } while (0)

    BruActionList *n;
    size_t         idx, len;

    if (!acts) return pc;

    for (n = acts->next; n != acts; n = n->next) {
        switch (n->act->type) {
            case BRU_ACT_BEGIN: BRU_BCWRITE(pc, BRU_BEGIN); break;
            case BRU_ACT_END: BRU_BCWRITE(pc, BRU_END); break;

            case BRU_ACT_CHAR:
                BRU_BCWRITE(pc, BRU_CHAR);
                BRU_MEMWRITE(pc, const char *, n->act->ch);
                break;

            case BRU_ACT_PRED:
                BRU_BCWRITE(pc, BRU_PRED);
                BRU_MEMWRITE(pc, bru_len_t, stc_vec_len_unsafe(prog->aux));
                BRU_MEMCPY(prog->aux, n->act->pred,
                           sizeof(*n->act->pred) +
                               n->act->pred->len *
                                   sizeof(*n->act->pred->intervals));
                break;

            case BRU_ACT_WRITE:
                switch (n->act->c) {
                    case '0': BRU_BCWRITE(pc, BRU_WRITE0); break;
                    case '1': BRU_BCWRITE(pc, BRU_WRITE1); break;
                    default:
                        BRU_BCWRITE(pc, BRU_WRITE);
                        BRU_MEMWRITE(pc, bru_byte_t, n->act->c);
                        break;
                }
                break;
            case BRU_ACT_MEMO:
                BRU_BCWRITE(pc, BRU_MEMO);
                GET_IDX(mmaps->memoisation_map, mmaps->next_memoisation_idx, 1,
                        n->act->k);
                BRU_MEMWRITE(pc, bru_len_t, idx);
                break;

            case BRU_ACT_EPSCHK:
                BRU_BCWRITE(pc, BRU_EPSCHK);
                GET_IDX(mmaps->thread_map, mmaps->next_thread_idx,
                        sizeof(const char *), n->act->k);
                BRU_MEMWRITE(pc, bru_len_t, idx);
                break;

            case BRU_ACT_SAVE:
                BRU_BCWRITE(pc, BRU_SAVE);
                BRU_MEMWRITE(pc, bru_len_t, n->act->k);
                if ((n->act->k / 2) + 1 > prog->ncaptures)
                    prog->ncaptures = (n->act->k / 2) + 1;
                break;

            case BRU_ACT_EPSSET:
                BRU_BCWRITE(pc, BRU_EPSSET);
                GET_IDX(mmaps->thread_map, mmaps->next_thread_idx,
                        sizeof(const char *), n->act->k);
                BRU_MEMWRITE(pc, bru_len_t, idx);
                break;
        }
    }

    return pc;

#undef GET_IDX
}

static size_t
count_bytes_transition(BruStateMachine *sm, bru_trans_id tid, int count_jmp)
{
    size_t size;

    size = count_bytes_actions(bru_smir_trans_get_actions(sm, tid));
    if (count_jmp) {
        size++;
        size += sizeof(bru_offset_t);
    }

    return size;
}

static bru_byte_t *compile_transition(BruStateMachine *sm,
                                      bru_byte_t      *pc,
                                      BruProgram      *prog,
                                      bru_trans_id     tid,
                                      int              compile_jmp,
                                      BruStateBlock   *state_blocks,
                                      BruMemoryMaps   *mmaps)
{
    const BruActionList *acts;
    bru_state_id         dst;
    bru_offset_t         jmp_target_idx;
    bru_offset_t         offset_idx;

    acts = bru_smir_trans_get_actions(sm, tid);
    dst  = bru_smir_get_dst(sm, tid);

    pc = compile_actions(pc, prog, acts, mmaps);

    if (compile_jmp) {
        BRU_BCWRITE(pc, BRU_JMP);
        offset_idx = pc - prog->insts;
        jmp_target_idx =
            BRU_IS_FINAL_STATE(dst)
                ? state_blocks[bru_smir_get_num_states(sm) + 1].entry
                : state_blocks[dst].entry;
        BRU_MEMWRITE(pc, bru_offset_t,
                     jmp_target_idx - (offset_idx + sizeof(bru_offset_t)));
    }

    return pc;
}

static size_t count_bytes_transitions(BruStateMachine *sm,
                                      bru_trans_id    *out,
                                      size_t           n,
                                      bru_state_id     sid)
{
    size_t i, nstates, size = 0;
    int    count_jmp;

    nstates = bru_smir_get_num_states(sm);
    for (i = 0; i < n - 1; i++)
        if (bru_smir_trans_get_num_actions(sm, out[i]))
            size += count_bytes_transition(sm, out[i], TRUE);

    // count bytes of last transition
    count_jmp = bru_smir_get_dst(sm, out[i]) != ((sid + 1) % (nstates + 1));
    if (n == 1 || bru_smir_trans_get_num_actions(sm, out[i]))
        size += count_bytes_transition(sm, out[i], count_jmp);

    return size;
}

static void compile_transitions(BruStateMachine *sm,
                                BruProgram      *prog,
                                bru_state_id     sid,
                                BruStateBlock   *state_blocks,
                                BruMemoryMaps   *mmaps)
{
    bru_trans_id *out;
    bru_byte_t   *pc;
    bru_state_id  dst;
    size_t        n, i, nstates;
    bru_offset_t  offset_idx;
    int           compile_jmp;

    nstates = bru_smir_get_num_states(sm);
    out     = bru_smir_get_out_transitions(sm, sid, &n);
    if (!n) goto cleanup;

    offset_idx = state_blocks[sid].exit;
    pc         = prog->insts + state_blocks[sid].transitions;
    for (i = 0; i < n - 1; offset_idx += sizeof(bru_offset_t), i++) {
        if (bru_smir_trans_get_num_actions(sm, out[i])) {
            SET_OFFSET(prog->insts, offset_idx, pc - prog->insts);
            pc = compile_transition(sm, pc, prog, out[i], TRUE, state_blocks,
                                    mmaps);
        } else {
            dst = bru_smir_get_dst(sm, out[i]);
            if (!dst) dst = nstates + 1;
            SET_OFFSET(prog->insts, offset_idx, state_blocks[dst].entry);
        }
    }

    // compile last transition
    compile_jmp = bru_smir_get_dst(sm, out[i]) != ((sid + 1) % (nstates + 1));
    if (n > 1) {
        if (bru_smir_trans_get_num_actions(sm, out[i])) {
            SET_OFFSET(prog->insts, offset_idx, pc - prog->insts);
            compile_transition(sm, pc, prog, out[i], compile_jmp, state_blocks,
                               mmaps);
        } else {
            dst = bru_smir_get_dst(sm, out[i]);
            if (!dst) dst = nstates + 1;
            SET_OFFSET(prog->insts, offset_idx, state_blocks[dst].entry);
        }
    } else if (n == 1) {
        compile_transition(sm, pc, prog, out[i], compile_jmp, state_blocks,
                           mmaps);
    }

cleanup:
    if (out) free(out);
}

static void compile_state(BruStateMachine *sm,
                          BruProgram      *prog,
                          bru_state_id     sid,
                          bru_compile_f   *pre,
                          bru_compile_f   *post,
                          BruStateBlock   *state_blocks,
                          BruMemoryMaps   *mmaps)
{
    bru_trans_id        *out;
    const BruActionList *acts;
    size_t               n, size;

    state_blocks[sid].entry = stc_vec_len_unsafe(prog->insts);

    if (pre) pre(bru_smir_get_pre_meta(sm, sid), prog);
    if (bru_smir_state_get_num_actions(sm, sid)) {
        acts = bru_smir_state_get_actions(sm, sid);
        size = count_bytes_actions(acts);
        RESERVE(prog->insts, size);
        compile_actions(prog->insts + stc_vec_len_unsafe(prog->insts) - size,
                        prog, acts, mmaps);
    }
    if (post) post(bru_smir_get_post_meta(sm, sid), prog);

    out = bru_smir_get_out_transitions(sm, sid, &n);
    switch (n) {
        case 0:
            state_blocks[sid].exit        = 0;
            state_blocks[sid].transitions = stc_vec_len_unsafe(prog->insts);
            goto cleanup;

        case 1:
            state_blocks[sid].exit        = stc_vec_len_unsafe(prog->insts);
            state_blocks[sid].transitions = state_blocks[sid].exit;
            break;

        default:
            if (n == 2) {
                BRU_BCPUSH(prog->insts, BRU_SPLIT);
            } else {
                BRU_BCPUSH(prog->insts, BRU_TSWITCH);
                BRU_MEMPUSH(prog->insts, bru_len_t, n);
            }
            state_blocks[sid].exit = stc_vec_len_unsafe(prog->insts);
            RESERVE(prog->insts, n * sizeof(bru_offset_t));
            state_blocks[sid].transitions = stc_vec_len_unsafe(prog->insts);
            break;
    }

    size = count_bytes_transitions(sm, out, n, sid);
    if (size) RESERVE(prog->insts, size);

cleanup:
    if (out) free(out);
}

static void compile_initial(BruStateMachine *sm,
                            BruProgram      *prog,
                            BruStateBlock   *state_blocks,
                            BruMemoryMaps   *mmaps)
{
    compile_state(sm, prog, BRU_INITIAL_STATE_ID, NULL, NULL, state_blocks,
                  mmaps);
}

/* --- API function definitions --------------------------------------------- */

BruProgram *bru_smir_compile_with_meta(BruStateMachine *sm,
                                       bru_compile_f   *pre,
                                       bru_compile_f   *post)
{
    BruProgram    *prog  = bru_program_default(sm->regex);
    BruMemoryMaps  mmaps = { 0 };
    BruStateBlock *state_blocks;
    size_t         n, sid;

    stc_vec_default_init(mmaps.thread_map);
    stc_vec_default_init(mmaps.memoisation_map);

    n            = bru_smir_get_num_states(sm);
    state_blocks = malloc((n + 2) * sizeof(*state_blocks));

    // compile `initial .. states .. final` states
    // and store entry, exit, and transitions offsets
    compile_initial(sm, prog, state_blocks, &mmaps);
    for (sid = 1; sid <= n; sid++)
        compile_state(sm, prog, sid, pre, post, state_blocks, &mmaps);
    state_blocks[sid].entry = stc_vec_len_unsafe(prog->insts);
    state_blocks[sid].exit  = 0;
    BRU_BCPUSH(prog->insts, BRU_MATCH);

    // compile out transitions for each state
    for (sid = 0; sid <= n; sid++)
        compile_transitions(sm, prog, sid, state_blocks, &mmaps);

    prog->thread_mem_len = mmaps.next_thread_idx;
    prog->nmemo_insts    = stc_vec_len_unsafe(mmaps.memoisation_map);

    // cleanup
    stc_vec_free(mmaps.thread_map);
    stc_vec_free(mmaps.memoisation_map);
    free(state_blocks);

    return prog;
}
