#include <assert.h>
#include <stdlib.h>

#include "stc/fatp/vec.h"

#include "smir.h"

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

static void action_list_free(ActionList *self)
{
    if (!self) return;

    free(self);
}

static void trans_free(Trans *self)
{
    if (!self) return;

    if (self->actions_sentinel) smir_action_list_free(self->actions_sentinel);
    free(self);
}

static void state_free(State *self)
{
    Trans *elem, *next;

    if (self->out_transitions_sentinel)
        dll_free(self->out_transitions_sentinel, trans_free, elem, next);
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

void smir_free(StateMachine *self)
{
    Trans *elem, *next;
    size_t nstates;

    if (!self) return;

    if (self->states) {
        nstates = stc_vec_len(self->states);
        while (nstates) state_free(&self->states[--nstates]);
        stc_vec_free(self->states);
    }

    if (self->initial_functions_sentinel)
        dll_free(self->initial_functions_sentinel, trans_free, elem, next);

    free(self);
}

Program *smir_compile(StateMachine *self)
{
    return smir_compile_with_meta(self, NULL, NULL);
}

state_id smir_add_state(StateMachine *self)
{
    state_id sid   = stc_vec_len(self->states);
    State    state = { 0 };

    dll_init(state.out_transitions_sentinel);
    stc_vec_push(self->states, state);

    return sid;
}

size_t smir_get_nstates(StateMachine *self)
{
    return stc_vec_len_unsafe(self->states);
}

trans_id smir_set_initial(StateMachine *self, state_id sid)
{
    Trans *transition;

    trans_init(transition);
    transition->dst = sid;
    dll_push_back(self->initial_functions_sentinel, transition);

    return trans_id_from_parts(NULL_STATE, self->ninits++);
}

trans_id *smir_get_initial(StateMachine *self, size_t *n)
{
    return smir_get_out_transitions(self, NULL_STATE, n);
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
    dll_push_back(self->states[sid - 1].out_transitions_sentinel, transition);

    return trans_id_from_parts(sid, self->states[sid - 1].nout++);
}

trans_id *smir_get_out_transitions(StateMachine *self, state_id sid, size_t *n)
{
    State    *state = &self->states[sid - 1];
    trans_id *tids  = malloc(state->nout * sizeof(tids[0]));
    size_t    i;

    for (i = 0; i < state->nout; i++) tids[i] = trans_id_from_parts(sid, i);
    if (n) *n = state->nout;

    return tids;
}

const Predicate *smir_get_predicate(StateMachine *self, state_id sid)
{
    return self->states[sid - 1].pred;
}

void smir_set_predicate(StateMachine *self, state_id sid, const Predicate *pred)
{
    self->states[sid - 1].pred = pred;
}

state_id smir_get_src(StateMachine *self, trans_id tid)
{
    (void) self;
    return trans_id_sid(tid);
}

state_id smir_get_dst(StateMachine *self, trans_id tid)
{
    State *state = &self->states[trans_id_sid(tid)];
    Trans *transition;
    size_t idx;

    idx = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);

    return transition->dst;
}

void smir_set_dst(StateMachine *self, trans_id tid, state_id dst)
{
    State *state = &self->states[trans_id_sid(tid)];
    Trans *transition;
    size_t idx;

    idx = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);
    transition->dst = dst;
}

const ActionList *smir_get_actions(StateMachine *self, trans_id tid)
{
    State *state = &self->states[trans_id_sid(tid)];
    Trans *transition;
    size_t idx;

    idx = trans_id_idx(tid);
    dll_get(state->out_transitions_sentinel, idx, transition);

    return transition->actions_sentinel;
}

void smir_append_action(StateMachine *self, trans_id tid, const Action *act)
{
    State      *state = &self->states[trans_id_sid(tid)];
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
    State      *state = &self->states[trans_id_sid(tid)];
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
    State *state = &self->states[trans_id_sid(tid)];
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
    void *old_meta = self->states[sid - 1].pre_meta;

    self->states[sid - 1].pre_meta = meta;

    return old_meta;
}

void *smir_get_pre_meta(StateMachine *self, state_id sid)
{
    return self->states[sid - 1].pre_meta;
}

void *smir_set_post_meta(StateMachine *self, state_id sid, void *meta)
{
    void *old_meta = self->states[sid - 1].post_meta;

    self->states[sid - 1].post_meta = meta;

    return old_meta;
}

void *smir_get_post_meta(StateMachine *self, state_id sid)
{
    return self->states[sid - 1].post_meta;
}

void smir_transform(StateMachine *self, transform_f transformer)
{
    assert(0 && "TODO");
}

/* --- SMIR Compilation ----------------------------------------------------- */

#define RESERVE(bytes, n)               \
    do {                                \
        stc_vec_reserve(bytes, n);      \
        stc_vec_len_unsafe(bytes) += n; \
    } while (0)

#define PC(insts) (insts) + stc_vec_len_unsafe(insts)

/* --- Data Structures ------------------------------------------------------ */

typedef struct {
    byte     *entry; /*<< where the state is compiled in the program */
    offset_t *exit;  /*<< the to-be-filled outgoing transition offsets */
} CompiledState;

/* --- Helper Routines ------------------------------------------------------ */

static void compile_predicate(Program *prog, const Predicate *pred)
{
    byte *insts;

    if (!pred) return;

    insts = prog->insts;

    switch (pred->type) {
        case CARET: BCWRITE(insts, BEGIN); break;
        case DOLLAR: BCWRITE(insts, END); break;
        case MEMOISE: BCWRITE(insts, MEMO); break;
        case LITERAL:
            // CHAR X
            BCWRITE(insts, CHAR);
            MEMWRITE(insts, const char *, pred->ch);
            break;
        case CC:
            // PRED N K
            BCWRITE(insts, PRED);
            MEMWRITE(insts, len_t, pred->cc_len);
            MEMWRITE(insts, len_t, stc_vec_len_unsafe(prog->aux));
            MEMCPY(prog->aux, pred->intervals, pred->cc_len * sizeof(Interval));
            break;

        case ALT:
        case CONCAT:
        case CAPTURE:
        case STAR:
        case PLUS:
        case QUES:
        case COUNTER:
        case LOOKAHEAD:
        case NREGEXTYPES: assert(0 && "Invalid predicate node!"); break;
    }
}

static void compile_actions(Program *prog, const ActionList *acts)
{
    ActionList *n;

    if (!acts) return;

    for (n = acts->next; n != acts; n = n->next) {
        switch (n->act->type) {
            case CARET:
            case DOLLAR:
            case MEMOISE:
            case LITERAL:
            case CC: compile_predicate(prog, n->act); break;

            case CAPTURE:
                BCWRITE(prog->insts, SAVE);
                MEMWRITE(prog->insts, len_t, n->act->capture_idx);
                break;

            case CONCAT:
            case ALT:
            case STAR:
            case PLUS:
            case QUES:
            case COUNTER:
            case LOOKAHEAD:
            case NREGEXTYPES: break;
        }
    }
}

static void compile_transition(StateMachine  *sm,
                               Program       *prog,
                               trans_id       tid,
                               CompiledState *compiled_states)
{
    state_id          dst;
    byte             *insts, *jmp_target;
    offset_t         *offset;
    const ActionList *acts;

    acts = smir_get_actions(sm, tid);
    dst  = smir_get_dst(sm, tid);

    compile_actions(prog, acts);

    BCWRITE(prog->insts, JMP);

    offset     = (offset_t *) PC(insts);
    jmp_target = IS_FINAL_STATE(dst)
                     ? compiled_states[smir_get_nstates(sm) + 1].entry
                     : compiled_states[dst].entry;
    MEMWRITE(insts, offset_t, jmp_target - (byte *) (offset + 1));
}

static void compile_state(StateMachine  *sm,
                          Program       *prog,
                          state_id       sid,
                          compile_f      pre,
                          compile_f      post,
                          CompiledState *compiled_states)
{
    trans_id *out;
    size_t    n, i;
    byte     *insts;

    insts                      = prog->insts;
    compiled_states[sid].entry = PC(insts);

    if (pre) pre(smir_get_pre_meta(sm, sid), prog);
    compile_predicate(prog, smir_get_predicate(sm, sid));
    if (post) post(smir_get_post_meta(sm, sid), prog);

    out = smir_get_out_transitions(sm, sid, &n);

    switch (n) {
        case 0: goto done;

        case 1: BCWRITE(insts, JMP); break;

        case 2: BCWRITE(insts, SPLIT); break;

        default:
            BCWRITE(insts, TSWITCH);
            MEMWRITE(insts, len_t, n);
            break;
    }

    compiled_states[sid].exit = (offset_t *) PC(insts);
    RESERVE(insts, n * sizeof(offset_t));

done:
    if (out) free(out);
    return;
}

static void compile_transitions(StateMachine  *sm,
                                Program       *prog,
                                state_id       sid,
                                CompiledState *compiled_states)
{
    trans_id *out;
    size_t    n, i;
    byte     *transition;
    offset_t *offset;

    offset = compiled_states[sid].exit;
    if (!offset) return;

    out = smir_get_out_transitions(sm, sid, &n);

    for (i = 0; i < n; offset++, i++) {
        *offset = PC(prog->insts) - (byte *) (offset + 1);
        compile_transition(sm, prog, out[i], compiled_states);
    }

    if (out) free(out);
}

static void
compile_initial(StateMachine *sm, Program *prog, CompiledState *compiled_states)
{
    compile_state(sm, prog, INITIAL_STATE_ID, NULL, NULL, compiled_states);
}

/* --- Main Routine --------------------------------------------------------- */

Program *smir_compile_with_meta(StateMachine *sm, compile_f pre, compile_f post)
{
    Program       *prog = program_default();
    size_t         n, sid;
    CompiledState *compiled_states;

    n = smir_get_nstates(sm);
    stc_vec_init(compiled_states, n + 2);

    // compile `initial .. states .. final` states
    // and store entry and exit ptrs
    compile_initial(sm, prog, compiled_states);
    for (sid = 1; sid <= n; sid++) {
        compile_state(sm, prog, sid, pre, post, compiled_states);
    }
    compiled_states[sid].entry = PC(prog->insts);
    compiled_states[sid].exit  = NULL;
    BCWRITE(prog->insts, MATCH);

    stc_vec_len_unsafe(compiled_states) = n + 2;

    // compile out transitions for each state
    for (sid = 1; sid <= n; sid++) {
        compile_transitions(sm, prog, sid, compiled_states);
    }

    // cleanup
    stc_vec_free(compiled_states);

    return prog;
}
