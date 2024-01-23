#include "thompson.h"
#include "smir.h"
#include <assert.h>

/* --- Macros --------------------------------------------------------------- */

#define WRAP_CHILD(sm, child, init, child_ids, fin, trans)    \
    do {                                                      \
        child_ids = emit((sm), (child), opts);                \
        trans     = smir_add_transition((sm), (init));        \
        smir_set_dst((sm), (trans), (child_ids).initial);     \
        trans = smir_add_transition((sm), (child_ids).final); \
        smir_set_dst((sm), (trans), (fin));                   \
    } while (0)

#define SET_TRANS_PRIORITY(sm, re, sid, enter, leave)   \
    do {                                                \
        if ((re)->pos) {                                \
            (enter) = smir_add_transition((sm), (sid)); \
            (leave) = smir_add_transition((sm), (sid)); \
        } else {                                        \
            (leave) = smir_add_transition((sm), (sid)); \
            (enter) = smir_add_transition((sm), (sid)); \
        }                                               \
    } while (0)

/* --- Data Structures ------------------------------------------------------ */

typedef struct {
    state_id initial;
    state_id final;
} state_id_pair;

/* --- Helper Prototypes ---------------------------------------------------- */

static state_id_pair
emit(StateMachine *sm, const RegexNode *re, const CompilerOpts *opts, len_t *k);

/* --- Main Routine --------------------------------------------------------- */

StateMachine *thompson_construct(Regex re, const CompilerOpts *opts)
{
    StateMachine *sm;
    state_id_pair child;
    len_t         k = 0;

    sm    = smir_default(re.regex);
    child = emit(sm, re.root, opts, &k);
    smir_set_initial(sm, child.initial);
    smir_set_final(sm, child.final);

    return sm;
}

/* --- Helper Routines ------------------------------------------------------ */

static state_id_pair
emit(StateMachine *sm, const RegexNode *re, const CompilerOpts *opts, len_t *k)
{
    state_id_pair state_ids, child_state_ids;
    trans_id      out, enter, leave;

    switch (re->type) {
        case CARET:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(sm, state_ids.final,
                                     smir_action_zwa(ACT_BEGIN));
            break;

        case DOLLAR:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(sm, state_ids.final,
                                     smir_action_zwa(ACT_END));
            break;

        case MEMOISE:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(sm, state_ids.final,
                                     smir_action_num(ACT_MEMO, *k));
            k += sizeof(byte *);
            break;

        case LITERAL:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(sm, state_ids.final,
                                     smir_action_char(re->ch));
            break;

        case CC:
            state_ids.initial = state_ids.final = smir_add_state(sm);
            smir_state_append_action(
                sm, state_ids.final,
                smir_action_predicate(
                    intervals_clone(re->intervals, re->cc_len), re->cc_len));
            break;

        case ALT:
            state_ids.initial = smir_add_state(sm);

            child_state_ids = emit(sm, re->left, opts, k);
            out             = smir_add_transition(sm, state_ids.initial);
            smir_set_dst(sm, out, child_state_ids.initial);
            out = smir_add_transition(sm, child_state_ids.final);

            child_state_ids = emit(sm, re->right, opts, k);
            leave           = smir_add_transition(sm, state_ids.initial);
            smir_set_dst(sm, leave, child_state_ids.initial);
            leave = smir_add_transition(sm, child_state_ids.final);

            state_ids.final = smir_add_state(sm);
            smir_set_dst(sm, out, state_ids.final);
            smir_set_dst(sm, leave, state_ids.final);
            break;

        case CONCAT:
            state_ids       = emit(sm, re->left, opts, k);
            child_state_ids = emit(sm, re->right, opts, k);

            out = smir_add_transition(sm, state_ids.final);
            smir_set_dst(sm, out, child_state_ids.initial);

            state_ids.final = child_state_ids.final;
            break;

        case CAPTURE:
            state_ids.initial = smir_add_state(sm);
            child_state_ids   = emit(sm, re->left, opts, k);
            state_ids.final   = smir_add_state(sm);

            out = smir_add_transition(sm, state_ids.initial);
            smir_set_dst(sm, out, child_state_ids.initial);
            smir_trans_append_action(
                sm, out, smir_action_num(ACT_SAVE, re->capture_idx));

            out = smir_add_transition(sm, child_state_ids.final);
            smir_set_dst(sm, out, state_ids.final);
            smir_trans_append_action(
                sm, out, smir_action_num(ACT_SAVE, re->capture_idx + 1));
            break;

        case STAR:
            state_ids.initial = smir_add_state(sm);
            child_state_ids   = emit(sm, re->left, opts, k);
            state_ids.final   = smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, state_ids.initial, enter, leave);
            smir_set_dst(sm, enter, child_state_ids.initial);
            smir_set_dst(sm, leave, state_ids.final);
            if (opts->capture_semantics == CS_PCRE)
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSSET, *k));

            SET_TRANS_PRIORITY(sm, re, child_state_ids.final, enter, leave);
            smir_set_dst(sm, enter, child_state_ids.initial);
            smir_set_dst(sm, leave, state_ids.final);
            smir_trans_append_action(sm, enter,
                                     smir_action_num(ACT_EPSCHK, *k));
            if (opts->capture_semantics == CS_RE2)
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSSET, *k));

            *k += sizeof(const char *);
            break;

        case PLUS:
            if (opts->capture_semantics == CS_PCRE) {
                state_ids.initial = smir_add_state(sm);
                child_state_ids   = emit(sm, re->left, opts, k);

                out = smir_add_transition(sm, state_ids.initial);
                smir_set_dst(sm, out, child_state_ids.initial);
                smir_trans_append_action(sm, out,
                                         smir_action_num(ACT_EPSSET, *k));
            } else {
                state_ids = child_state_ids = emit(sm, re->left, opts, k);
            }
            state_ids.final = smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, child_state_ids.final, enter, leave);
            smir_set_dst(sm, enter, child_state_ids.initial);
            smir_set_dst(sm, leave, state_ids.final);
            smir_trans_append_action(sm, enter,
                                     smir_action_num(ACT_EPSCHK, *k));
            if (opts->capture_semantics == CS_RE2)
                smir_trans_append_action(sm, enter,
                                         smir_action_num(ACT_EPSSET, *k));

            *k += sizeof(const char *);
            break;

        case QUES:
            state_ids.initial = smir_add_state(sm);
            child_state_ids   = emit(sm, re->left, opts, k);
            state_ids.final   = smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, state_ids.initial, enter, leave);
            smir_set_dst(sm, enter, child_state_ids.initial);
            smir_set_dst(sm, leave, state_ids.final);

            out = smir_add_transition(sm, child_state_ids.final);
            smir_set_dst(sm, out, state_ids.final);
            break;

        case COUNTER:
        case LOOKAHEAD: assert(0 && "TODO"); break;
        case NREGEXTYPES: assert(0 && "unreachable"); break;
    }

    return state_ids;
}
