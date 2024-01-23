#include <assert.h>
#include <stdio.h>
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

struct action {
    ActionType type;

    union {
        const char *ch;   /*<< type = ACT_CHAR */
        Interval   *pred; /*<< type = ACT_PRED */
    };

    union {
        len_t pred_len; /*<< type = ACT_PRED */
        len_t k; /*<< type = ACT_SAVE | ACT_EPSSET | ACT_EPSCHK | ACT_MEMO */
    };
};

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
    const char *regex;
    State      *states;
    Trans      *initial_functions_sentinel;
    size_t      ninits;
};

/* --- Helper Functions ----------------------------------------------------- */

static void action_free(const Action *self)
{
    if (!self) return;

    if (self->type == ACT_PRED) free(self->pred);
    free((Action *) self);
}

static void action_list_free(ActionList *self)
{
    if (!self) return;

    action_free(self->act);
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

    action_free(self->pred);

    if (self->out_transitions_sentinel)
        dll_free(self->out_transitions_sentinel, trans_free, elem, next);
}

/* --- API ------------------------------------------------------------------ */

StateMachine *smir_default(const char *regex)
{
    StateMachine *sm = malloc(sizeof(*sm));

    sm->regex  = regex;
    sm->ninits = 0;
    stc_vec_default_init(sm->states);
    dll_init(sm->initial_functions_sentinel);

    return sm;
}

StateMachine *smir_new(const char *regex, uint32_t nstates)
{
    StateMachine *sm = malloc(sizeof(*sm));

    sm->regex  = regex;
    sm->ninits = 0;
    stc_vec_init(sm->states, nstates);
    dll_init(sm->initial_functions_sentinel);

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
    stc_vec_push_back(self->states, state);

    return sid + 1;
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
    Trans  *transition, *transitions;
    size_t *n;

    trans_init(transition);
    transition->src = sid;
    if (sid) {
        transitions = self->states[sid - 1].out_transitions_sentinel;
        n           = &self->states[sid - 1].nout;
    } else {
        transitions = self->initial_functions_sentinel;
        n           = &self->ninits;
    }
    dll_push_back(transitions, transition);

    return trans_id_from_parts(sid, (*n)++);
}

trans_id *smir_get_out_transitions(StateMachine *self, state_id sid, size_t *n)
{
    trans_id *tids;
    size_t    i, m;

    m    = sid ? self->states[sid - 1].nout : self->ninits;
    tids = malloc(m * sizeof(*tids));
    for (i = 0; i < m; i++) tids[i] = trans_id_from_parts(sid, i);
    if (n) *n = m;

    return tids;
}

const Predicate *smir_get_predicate(StateMachine *self, state_id sid)
{
    return sid ? self->states[sid - 1].pred : NULL;
}

void smir_set_predicate(StateMachine *self, state_id sid, const Predicate *pred)
{
    if (sid) self->states[sid - 1].pred = pred;
}

state_id smir_get_src(StateMachine *self, trans_id tid)
{
    (void) self;
    return trans_id_sid(tid);
}

state_id smir_get_dst(StateMachine *self, trans_id tid)
{
    Trans   *transition, *transitions;
    uint32_t idx = trans_id_idx(tid);
    state_id sid = trans_id_sid(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    dll_get(transitions, idx, transition);

    return transition->dst;
}

void smir_set_dst(StateMachine *self, trans_id tid, state_id dst)
{
    Trans   *transition, *transitions;
    uint32_t idx = trans_id_idx(tid);
    state_id sid = trans_id_sid(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    dll_get(transitions, idx, transition);
    transition->dst = dst;
}

const Action *smir_action_zwa(ActionType type)
{
    Action *act = malloc(sizeof(*act));

    assert(type == ACT_BEGIN || type == ACT_END);
    act->type = type;

    return act;
}

const Action *smir_action_char(const char *ch)
{
    Action *act = malloc(sizeof(*act));

    act->type = ACT_CHAR;
    act->ch   = ch;

    return act;
}

const Action *smir_action_predicate(Interval *pred, len_t pred_len)
{
    Action *act = malloc(sizeof(*act));

    act->type     = ACT_PRED;
    act->pred     = pred;
    act->pred_len = pred_len;

    return act;
}

const Action *smir_action_num(ActionType type, len_t k)
{
    Action *act = malloc(sizeof(*act));

    act->type = type;
    act->k    = k;

    return act;
}

size_t smir_get_num_actions(StateMachine *self, trans_id tid)
{
    Trans   *transition, *transitions;
    uint32_t idx = trans_id_idx(tid);
    state_id sid = trans_id_sid(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    dll_get(transitions, idx, transition);

    return transition->nactions;
}

const ActionList *smir_get_actions(StateMachine *self, trans_id tid)
{
    Trans   *transition, *transitions;
    uint32_t idx = trans_id_idx(tid);
    state_id sid = trans_id_sid(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    dll_get(transitions, idx, transition);

    return transition->actions_sentinel;
}

void smir_append_action(StateMachine *self, trans_id tid, const Action *act)
{
    Trans      *transition, *transitions;
    ActionList *al  = malloc(sizeof(*al));
    uint32_t    idx = trans_id_idx(tid);
    state_id    sid = trans_id_sid(tid);

    al->act     = act;
    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    dll_get(transitions, idx, transition);
    dll_push_back(transition->actions_sentinel, al);
    transition->nactions++;
}

void smir_prepend_action(StateMachine *self, trans_id tid, const Action *act)
{
    Trans      *transition, *transitions;
    ActionList *al  = malloc(sizeof(*al));
    uint32_t    idx = trans_id_idx(tid);
    state_id    sid = trans_id_sid(tid);

    al->act     = act;
    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    dll_get(transitions, idx, transition);
    dll_push_front(transition->actions_sentinel, al);
    transition->nactions++;
}

void smir_set_actions(StateMachine *self, trans_id tid, ActionList *acts)
{
    Trans      *transition, *transitions;
    ActionList *al  = malloc(sizeof(*al));
    uint32_t    idx = trans_id_idx(tid);
    state_id    sid = trans_id_sid(tid);

    transitions = sid ? self->states[sid - 1].out_transitions_sentinel
                      : self->initial_functions_sentinel;
    dll_get(transitions, idx, transition);
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
    dll_free(self, action_list_free, elem, next);
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
    void *old_meta;

    if (!sid) return NULL;

    old_meta                       = self->states[sid - 1].pre_meta;
    self->states[sid - 1].pre_meta = meta;

    return old_meta;
}

void *smir_get_pre_meta(StateMachine *self, state_id sid)
{
    return sid ? self->states[sid - 1].pre_meta : NULL;
}

void *smir_set_post_meta(StateMachine *self, state_id sid, void *meta)
{
    void *old_meta;

    if (!sid) return NULL;

    old_meta                        = self->states[sid - 1].post_meta;
    self->states[sid - 1].post_meta = meta;

    return old_meta;
}

void *smir_get_post_meta(StateMachine *self, state_id sid)
{
    return sid ? self->states[sid - 1].post_meta : NULL;
}

void smir_transform(StateMachine *self, transform_f transformer)
{
    (void) self;
    (void) transformer;
    assert(0 && "TODO");
}

/* --- SMIR Compilation ----------------------------------------------------- */

#define RESERVE(bytes, n)               \
    do {                                \
        stc_vec_reserve(bytes, n);      \
        stc_vec_len_unsafe(bytes) += n; \
    } while (0)

#define PC(insts) ((insts) + stc_vec_len_unsafe(insts))

#define SET_OFFSET(insts, offset_idx, idx)     \
    *((offset_t *) ((insts) + (offset_idx))) = \
        (offset_t) (idx) - ((offset_idx) + sizeof(offset_t))

/* --- Data Structures ------------------------------------------------------ */

typedef struct {
    offset_t entry;     /*<< where the state is compiled in the program       */
    offset_t exit;      /*<< the to-be-filled outgoing transition offsets     */
    size_t transitions; /*<< where the transitions start                      */
} CompiledState;

/* --- Helper Routines ------------------------------------------------------ */

static size_t count_bytes_predicate(const Predicate *pred)
{
    size_t size = 0;

    switch (pred->type) {
        case ACT_BEGIN: size++; break;
        case ACT_END: size++; break;

        case ACT_CHAR:
            size++;
            size += sizeof(const char *);
            break;

        case ACT_PRED:
            size++;
            size += sizeof(len_t);
            size += sizeof(len_t);
            break;

        case ACT_MEMO:
            size++;
            size += sizeof(len_t);
            break;

        case ACT_EPSCHK:
            size++;
            size += sizeof(len_t);
            break;

        case ACT_SAVE:
        case ACT_EPSSET: assert(0 && "Invalid predicate node!"); break;
    }

    return size;
}

static byte *compile_predicate(byte *pc, Program *prog, const Predicate *pred)
{
    switch (pred->type) {
        case ACT_BEGIN: BCWRITE(pc, BEGIN); break;
        case ACT_END: BCWRITE(pc, END); break;

        case ACT_CHAR:
            BCWRITE(pc, CHAR);
            MEMWRITE(pc, const char *, pred->ch);
            break;

        case ACT_PRED:
            BCWRITE(pc, PRED);
            MEMWRITE(pc, len_t, pred->pred_len);
            MEMWRITE(pc, len_t, stc_vec_len_unsafe(prog->aux));
            MEMCPY(prog->aux, pred->pred, pred->pred_len * sizeof(Interval));
            break;

        case ACT_MEMO:
            BCWRITE(pc, MEMO);
            MEMWRITE(pc, len_t, pred->k);
            break;

        case ACT_EPSCHK:
            BCWRITE(pc, EPSCHK);
            MEMWRITE(pc, len_t, pred->k);
            break;

        case ACT_SAVE:
        case ACT_EPSSET: assert(0 && "Invalid predicate node!"); break;
    }

    return pc;
}

static size_t count_bytes_actions(const ActionList *acts)
{
    ActionList *n;
    size_t      size;

    if (!acts) return 0;

    for (n = acts->next, size = 0; n != acts; n = n->next) {
        switch (n->act->type) {
            case ACT_BEGIN:
            case ACT_END:
            case ACT_CHAR:
            case ACT_PRED:
            case ACT_EPSCHK:
            case ACT_MEMO: size = count_bytes_predicate(n->act); break;

            case ACT_SAVE:
                size++;
                size += sizeof(len_t);
                break;

            case ACT_EPSSET:
                size++;
                size += sizeof(len_t);
                break;
        }
    }

    return size;
}

static byte *compile_actions(byte *pc, Program *prog, const ActionList *acts)
{
    ActionList *n;

    if (!acts) return pc;

    for (n = acts->next; n != acts; n = n->next) {
        switch (n->act->type) {
            case ACT_BEGIN:
            case ACT_END:
            case ACT_CHAR:
            case ACT_PRED:
            case ACT_EPSCHK:
            case ACT_MEMO: pc = compile_predicate(pc, prog, n->act); break;

            case ACT_SAVE:
                BCWRITE(pc, SAVE);
                MEMWRITE(pc, len_t, n->act->k);
                break;

            case ACT_EPSSET:
                BCWRITE(pc, EPSSET);
                MEMWRITE(pc, len_t, n->act->k);
                break;
        }
    }

    return pc;
}

static size_t count_bytes_transition(StateMachine *sm, trans_id tid)
{
    size_t size;

    size = count_bytes_actions(smir_get_actions(sm, tid));
    size++;
    size += sizeof(offset_t);

    return size;
}

static byte *compile_transition(StateMachine  *sm,
                                byte          *pc,
                                Program       *prog,
                                trans_id       tid,
                                CompiledState *compiled_states)
{
    const ActionList *acts;
    state_id          dst;
    offset_t          jmp_target_idx;
    offset_t          offset_idx;

    acts = smir_get_actions(sm, tid);
    dst  = smir_get_dst(sm, tid);

    pc = compile_actions(pc, prog, acts);

    BCWRITE(pc, JMP);
    offset_idx     = pc - prog->insts;
    jmp_target_idx = IS_FINAL_STATE(dst)
                         ? compiled_states[smir_get_nstates(sm) + 1].entry
                         : compiled_states[dst].entry;
    MEMWRITE(pc, offset_t, jmp_target_idx - (offset_idx + sizeof(offset_t)));

    return pc;
}

static void compile_state(StateMachine  *sm,
                          Program       *prog,
                          state_id       sid,
                          compile_f      pre,
                          compile_f      post,
                          CompiledState *compiled_states)
{
    trans_id     *out;
    const Action *pred;
    size_t        i, n, size;

    compiled_states[sid].entry = stc_vec_len_unsafe(prog->insts);

    if (pre) pre(smir_get_pre_meta(sm, sid), prog);
    if ((pred = smir_get_predicate(sm, sid))) {
        size = count_bytes_predicate(pred);
        RESERVE(prog->insts, size);
        compile_predicate(prog->insts + stc_vec_len_unsafe(prog->insts) - size,
                          prog, pred);
    }
    if (post) post(smir_get_post_meta(sm, sid), prog);

    out = smir_get_out_transitions(sm, sid, &n);

    switch (n) {
        case 0: compiled_states[sid].exit = 0; goto done;

        case 1: BCPUSH(prog->insts, JMP); break;

        case 2: BCPUSH(prog->insts, SPLIT); break;

        default:
            BCPUSH(prog->insts, TSWITCH);
            MEMPUSH(prog->insts, len_t, n);
            break;
    }

    compiled_states[sid].exit = stc_vec_len_unsafe(prog->insts);
    RESERVE(prog->insts, n * sizeof(offset_t));

    for (i = 0, size = 0; i < n; i++)
        if (smir_get_num_actions(sm, out[i]))
            size += count_bytes_transition(sm, out[i]);

    compiled_states[sid].transitions = stc_vec_len_unsafe(prog->insts);
    RESERVE(prog->insts, size);

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
    byte     *pc;
    size_t    n, i;
    offset_t  offset_idx;

    offset_idx = compiled_states[sid].exit;
    if (!offset_idx) return;

    out = smir_get_out_transitions(sm, sid, &n);
    pc  = prog->insts + compiled_states[sid].transitions;
    for (i = 0; i < n; offset_idx += sizeof(offset_t), i++) {
        if (smir_get_num_actions(sm, out[i])) {
            SET_OFFSET(prog->insts, offset_idx, pc - prog->insts);
            pc = compile_transition(sm, pc, prog, out[i], compiled_states);
        } else {
            sid = smir_get_dst(sm, out[i]);
            if (!sid) sid = smir_get_nstates(sm) + 1;
            SET_OFFSET(prog->insts, offset_idx, compiled_states[sid].entry);
        }
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
    Program       *prog = program_default(sm->regex);
    CompiledState *compiled_states;
    size_t         n, sid;

    n = smir_get_nstates(sm);
    stc_vec_init(compiled_states, n + 2);

    // compile `initial .. states .. final` states
    // and store entry and exit ptrs
    compile_initial(sm, prog, compiled_states);
    for (sid = 1; sid <= n; sid++)
        compile_state(sm, prog, sid, pre, post, compiled_states);
    compiled_states[sid].entry = stc_vec_len_unsafe(prog->insts);
    compiled_states[sid].exit  = 0;
    BCPUSH(prog->insts, MATCH);

    stc_vec_len_unsafe(compiled_states) = n + 2;

    // compile out transitions for each state
    for (sid = 0; sid <= n; sid++)
        compile_transitions(sm, prog, sid, compiled_states);

    // cleanup
    stc_vec_free(compiled_states);

    return prog;
}
