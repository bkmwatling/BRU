#include <assert.h>
#include <stdlib.h>

#include "smir.h"
#include "stc/fatp/vec.h"

/* --- Macros --------------------------------------------------------------- */

#define trans_id_from_parts(sid, idx) ((((trans_id) sid) << 32) | idx)

#define trans_id_sid(tid) ((state_id) (tid >> 32))

#define trans_id_idx(tid) ((uint32_t) (tid & 0xffffffff))

#define trans_init(trans)                    \
    do {                                     \
        dll_init(trans);                     \
        dll_init((trans)->actions_sentinel); \
    } while (0)

#define dll_init(dll)                            \
    do {                                         \
        (dll)       = calloc(1, sizeof(*(dll))); \
        (dll)->prev = (dll)->next = (dll);       \
    } while (0)

#define dll_free(dll, elem_free, elem, next)                           \
    do {                                                               \
        for ((elem) = (dll)->next; (elem) != (dll); (elem) = (next)) { \
            (next) = (elem)->next;                                     \
            (elem_free)((elem));                                       \
        }                                                              \
        free((dll));                                                   \
    } while (0)

#define dll_push_front(dll, elem)         \
    do {                                  \
        (elem)->prev       = (dll);       \
        (elem)->next       = (dll)->next; \
        (elem)->next->prev = (elem);      \
        (dll)->next        = (elem);      \
    } while (0)

#define dll_push_back(dll, elem)          \
    do {                                  \
        (elem)->prev       = (dll)->prev; \
        (elem)->next       = (dll);       \
        (dll)->prev        = (elem);      \
        (elem)->prev->next = (elem);      \
    } while (0)

#define dll_get(dll, idx, elem)                              \
    for ((elem) = (dll)->next; (idx) > 0 && (elem) != (dll); \
         (idx)--, (elem) = (elem)->next)

/* --- Type Definitions ----------------------------------------------------- */

typedef struct trans Trans;

typedef struct {
    const Predicate *pred;
    Trans           *out_transitions_sentinel;
    size_t           nout;
    Trans           *in_transitions_sentinel;
    size_t           nin;
    void            *pre_meta;
    void            *post_meta;
} State;

struct trans {
    ActionList *actions_sentinel;
    size_t      nactions;
    state_id    src;
    state_id    dst;
    Trans      *prev;
    Trans      *next;
};

struct action_list {
    const Action *act;
    ActionList   *prev;
    ActionList   *next;
};

struct state_machine {
    State *states;
    Trans *initial_functions_sentinel;
    size_t ninits;
};

/* --- Helper Functions ----------------------------------------------------- */

static void action_list_free(ActionList *al)
{
    if (!al) return;

    free(al);
}

static void trans_free(Trans *transition)
{
    ActionList *elem, *next;

    if (!transition) return;

    if (transition->actions_sentinel) {
        dll_free(transition->actions_sentinel, action_list_free, elem, next);
    }

    free(transition);
}

static void state_free(State s)
{
    Trans *elem, *next;

    if (s.out_transitions_sentinel) {
        dll_free(s.out_transitions_sentinel, trans_free, elem, next);
    }

    // TODO: free in_transitions_sentinel?
}

/* --- API ------------------------------------------------------------------ */

StateMachine *smir_default(void)
{
    StateMachine *sm = malloc(sizeof(*sm));

    sm->initial_functions_sentinel = NULL;
    sm->states                     = NULL;
    stc_vec_default_init(sm->states);

    return sm;
}

StateMachine *smir_new(uint32_t nstates)
{
    StateMachine *sm = malloc(sizeof(*sm));

    sm->initial_functions_sentinel = NULL;
    sm->states                     = NULL;
    stc_vec_init(sm->states, nstates);

    return sm;
}

Program *smir_compile(StateMachine *self) {
    return smir_compile_with_meta(self, NULL, NULL);
}

void smir_free(StateMachine *self)
{
    Trans *elem, *next;
    size_t nstates;

    if (!self) return;

    if (self->states) {
        nstates = stc_vec_len(self->states);
        stc_vec_free(self->states);

        while (nstates--) { state_free(self->states[nstates]); }
    }

    if (self->initial_functions_sentinel) {
        dll_free(self->initial_functions_sentinel, trans_free, elem, next);
    }

    free(self);
}

state_id smir_add_state(StateMachine *self)
{
    state_id sid   = stc_vec_len(self->states);
    State    state = { 0 };

    dll_init(state.out_transitions_sentinel);
    dll_init(state.in_transitions_sentinel);
    stc_vec_push(self->states, state);

    return sid;
}

trans_id smir_set_initial(StateMachine *self, state_id sid)
{
    Trans *transition;

    trans_init(transition);
    transition->dst = sid;
    dll_push_back(self->initial_functions_sentinel, transition);

    return trans_id_from_parts(NULL_STATE, self->ninits++);
}

trans_id smir_set_final(StateMachine *self, state_id sid)
{
    return smir_add_transition(self, sid);
}

trans_id smir_add_transition(StateMachine *self, state_id sid)
{
    Trans *transition;

    trans_init(transition);
    transition->src = sid;
    dll_push_back(self->states[sid].out_transitions_sentinel, transition);

    return trans_id_from_parts(sid, self->states[sid].nout++);
}

trans_id *smir_get_out_transitions(StateMachine *self, state_id sid, size_t *n)
{
    State    *state = self->states + sid;
    trans_id *tids  = malloc(state->nout * sizeof(tids[0]));
    size_t    i;

    for (i = 0; i < state->nout; i++) tids[i] = trans_id_from_parts(sid, i);
    if (n) *n = i;

    return tids;
}

trans_id *smir_get_in_transitions(StateMachine *self, state_id sid, size_t *n)
{
    /* NOTE: do we even need this */
    assert(0 && "TODO");
}

const Predicate *smir_get_predicate(StateMachine *self, state_id sid)
{
    return self->states[sid].pred;
}

void smir_set_predicate(StateMachine *self, state_id sid, const Predicate *pred)
{
    self->states[sid].pred = pred;
}

state_id smir_get_src(StateMachine *self, trans_id tid)
{
    return trans_id_sid(tid);
}

state_id smir_get_dst(StateMachine *self, trans_id tid)
{
    State *state = self->states + trans_id_sid(tid);
    Trans *transition;
    size_t idx;

    idx = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);

    return transition->dst;
}

void smir_set_dst(StateMachine *self, trans_id tid, state_id dst)
{
    State *state = self->states + trans_id_sid(tid);
    Trans *transition;
    size_t idx;

    idx = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);

    /* TODO: update incoming transition of old dst and add to incoming
     * transitions of new dst */

    transition->dst = dst;
}

const ActionList *smir_get_actions(StateMachine *self, trans_id tid)
{
    State *state = self->states + trans_id_sid(tid);
    Trans *transition;
    size_t idx;

    idx = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);

    return transition->actions_sentinel;
}

void smir_append_action(StateMachine *self, trans_id tid, const Action *act)
{
    State      *state = self->states + trans_id_sid(tid);
    ActionList *al    = malloc(sizeof(*al));
    Trans      *transition;
    size_t      idx;

    al->act = act;
    idx     = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);
    dll_push_back(transition->actions_sentinel, al);
    transition->nactions++;
}

void smir_prepend_action(StateMachine *self, trans_id tid, const Action *act)
{
    State      *state = self->states + trans_id_sid(tid);
    ActionList *al    = malloc(sizeof(*al));
    Trans      *transition;
    size_t      idx;

    al->act = act;
    idx     = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);
    dll_push_front(transition->actions_sentinel, al);
    transition->nactions++;
}

void smir_set_actions(StateMachine *self, trans_id tid, ActionList *acts)
{
    State *state = self->states + trans_id_sid(tid);
    Trans *transition;
    size_t idx;

    idx = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);
    smir_action_list_free(transition->actions_sentinel);
    transition->actions_sentinel = acts;
    for (transition->nactions = 0, acts = acts->next;
         acts != transition->actions_sentinel;
         transition->nactions++, acts = acts->next)
        ;
}

ActionList *smir_action_list_new(void)
{
    ActionList *action_list;

    dll_init(action_list);

    return action_list;
}

void smir_action_list_free(ActionList *self)
{
    ActionList *elem, *next;
    dll_free(self, free, elem, next);
}

void smir_action_list_append(ActionList *self, const Action *act)
{
    ActionList *al = malloc(sizeof(*al));

    al->act = act;
    dll_push_back(self, al);
}

void smir_action_list_prepend(ActionList *self, const Action *act)
{
    ActionList *al = malloc(sizeof(*al));

    al->act = act;
    dll_push_front(self, al);
}

/* --- Extendable API ------------------------------------------------------- */

void *smir_set_pre_meta(StateMachine *self, state_id sid, void *meta)
{
    void *old_meta = self->states[sid].pre_meta;

    self->states[sid].pre_meta = meta;

    return old_meta;
}

void *smir_get_pre_meta(StateMachine *self, state_id sid)
{
    return self->states[sid].pre_meta;
}

void *smir_set_post_meta(StateMachine *self, state_id sid, void *meta)
{
    void *old_meta = self->states[sid].post_meta;

    self->states[sid].post_meta = meta;

    return old_meta;
}

void *smir_get_post_meta(StateMachine *self, state_id sid)
{
    return self->states[sid].post_meta;
}

Program *smir_compile_with_meta(StateMachine *self,
                                compile_f     pre_meta,
                                compile_f     post_meta)
{
    assert(0 && "TODO");
}

void smir_transform(StateMachine *self, transform_f transformer)
{
    assert(0 && "TODO");
}
