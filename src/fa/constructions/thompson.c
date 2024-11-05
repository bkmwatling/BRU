#include <assert.h>

#include "../../re/sre.h"
#include "thompson.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define SET_TRANS_PRIORITY(sm, re, sid, enter, leave)       \
    do {                                                    \
        if ((re)->greedy) {                                 \
            (enter) = bru_smir_add_transition((sm), (sid)); \
            (leave) = bru_smir_add_transition((sm), (sid)); \
        } else {                                            \
            (leave) = bru_smir_add_transition((sm), (sid)); \
            (enter) = bru_smir_add_transition((sm), (sid)); \
        }                                                   \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    bru_state_id initial;
    bru_state_id final;
} BruStateIdPair;

/* --- Helper function prototypes ------------------------------------------- */

static BruStateIdPair
emit(BruStateMachine *sm, const BruRegexNode *re, const BruCompilerOpts *opts);

/* --- API function definitions --------------------------------------------- */

BruStateMachine *bru_thompson_construct(BruRegex               re,
                                        const BruCompilerOpts *opts)
{
    BruStateMachine *sm;
    BruStateIdPair   child;

    sm    = bru_smir_default(re.regex);
    child = emit(sm, re.root, opts);
    bru_smir_set_initial(sm, child.initial);
    bru_smir_set_final(sm, child.final);

    return sm;
}

/* --- Helper functions ----------------------------------------------------- */

static BruStateIdPair
emit(BruStateMachine *sm, const BruRegexNode *re, const BruCompilerOpts *opts)
{
    BruStateIdPair state_ids, child_state_ids;
    bru_trans_id   out, enter, leave;
    bru_state_id   sid;

    switch (re->type) {
        case BRU_EPSILON:
            state_ids.initial = state_ids.final = bru_smir_add_state(sm);
            break;

        case BRU_CARET:
            state_ids.initial = state_ids.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(sm, state_ids.final,
                                         bru_smir_action_zwa(BRU_ACT_BEGIN));
            break;

        case BRU_DOLLAR:
            state_ids.initial = state_ids.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(sm, state_ids.final,
                                         bru_smir_action_zwa(BRU_ACT_END));
            break;

        // case MEMOISE:
        //     state_ids.initial = state_ids.final = smir_add_state(sm);
        //     smir_state_append_action(sm, state_ids.final,
        //                              smir_action_num(ACT_MEMO, re->rid));
        //     break;
        //
        case BRU_LITERAL:
            state_ids.initial = state_ids.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(sm, state_ids.final,
                                         bru_smir_action_char(re->ch));
            break;

        case BRU_CC:
            state_ids.initial = state_ids.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(
                sm, state_ids.final,
                bru_smir_action_predicate(bru_intervals_clone(re->intervals)));
            break;

        case BRU_ALT:
            state_ids.initial = bru_smir_add_state(sm);

            child_state_ids = emit(sm, re->left, opts);
            out             = bru_smir_add_transition(sm, state_ids.initial);
            bru_smir_set_dst(sm, out, child_state_ids.initial);
            out = bru_smir_add_transition(sm, child_state_ids.final);

            child_state_ids = emit(sm, re->right, opts);
            leave           = bru_smir_add_transition(sm, state_ids.initial);
            bru_smir_set_dst(sm, leave, child_state_ids.initial);
            leave = bru_smir_add_transition(sm, child_state_ids.final);

            state_ids.final = bru_smir_add_state(sm);
            bru_smir_set_dst(sm, out, state_ids.final);
            bru_smir_set_dst(sm, leave, state_ids.final);
            break;

        case BRU_CONCAT:
            state_ids       = emit(sm, re->left, opts);
            child_state_ids = emit(sm, re->right, opts);

            out = bru_smir_add_transition(sm, state_ids.final);
            bru_smir_set_dst(sm, out, child_state_ids.initial);

            state_ids.final = child_state_ids.final;
            break;

        case BRU_CAPTURE:
            state_ids.initial = bru_smir_add_state(sm);
            child_state_ids   = emit(sm, re->left, opts);
            state_ids.final   = bru_smir_add_state(sm);

            out = bru_smir_add_transition(sm, state_ids.initial);
            bru_smir_set_dst(sm, out, child_state_ids.initial);
            bru_smir_trans_append_action(
                sm, out,
                bru_smir_action_num(BRU_ACT_SAVE, 2 * re->capture_idx));

            out = bru_smir_add_transition(sm, child_state_ids.final);
            bru_smir_set_dst(sm, out, state_ids.final);
            bru_smir_trans_append_action(
                sm, out,
                bru_smir_action_num(BRU_ACT_SAVE, 2 * re->capture_idx + 1));
            break;

        case BRU_STAR:
            state_ids.initial = bru_smir_add_state(sm);
            if (opts->capture_semantics == BRU_CS_PCRE)
                sid = bru_smir_add_state(sm);
            child_state_ids = emit(sm, re->left, opts);
            if (opts->capture_semantics == BRU_CS_RE2)
                sid = child_state_ids.initial;
            state_ids.final = bru_smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, state_ids.initial, enter, leave);
            bru_smir_set_dst(sm, enter, sid);
            bru_smir_set_dst(sm, leave, state_ids.final);
            if (opts->capture_semantics == BRU_CS_PCRE) {
                enter = bru_smir_add_transition(sm, sid);
                bru_smir_set_dst(sm, enter, child_state_ids.initial);
                bru_smir_trans_append_action(
                    sm, enter, bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
            }

            SET_TRANS_PRIORITY(sm, re, child_state_ids.final, enter, leave);
            bru_smir_set_dst(sm, enter, sid);
            bru_smir_set_dst(sm, leave, state_ids.final);
            if (opts->capture_semantics == BRU_CS_PCRE) {
                bru_smir_trans_append_action(
                    sm, enter, bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));
            } else if (opts->capture_semantics == BRU_CS_RE2) {
                bru_smir_state_append_action(
                    sm, child_state_ids.final,
                    bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));
                bru_smir_trans_append_action(
                    sm, enter, bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
            }
            break;

        case BRU_PLUS:
            if (opts->capture_semantics == BRU_CS_PCRE) {
                state_ids.initial = bru_smir_add_state(sm);
                child_state_ids   = emit(sm, re->left, opts);

                out = bru_smir_add_transition(sm, state_ids.initial);
                bru_smir_set_dst(sm, out, child_state_ids.initial);
                bru_smir_trans_append_action(
                    sm, out, bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
            } else if (opts->capture_semantics == BRU_CS_RE2) {
                state_ids = child_state_ids = emit(sm, re->left, opts);
            }
            state_ids.final = bru_smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, child_state_ids.final, enter, leave);
            bru_smir_set_dst(sm, enter, state_ids.initial);
            bru_smir_set_dst(sm, leave, state_ids.final);
            if (opts->capture_semantics == BRU_CS_PCRE) {
                bru_smir_trans_append_action(
                    sm, enter, bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));
            } else if (opts->capture_semantics == BRU_CS_RE2) {
                bru_smir_state_append_action(
                    sm, child_state_ids.final,
                    bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));
                bru_smir_trans_append_action(
                    sm, enter, bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
            }
            break;

        case BRU_QUES:
            state_ids.initial = bru_smir_add_state(sm);
            child_state_ids   = emit(sm, re->left, opts);
            state_ids.final   = bru_smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, state_ids.initial, enter, leave);
            bru_smir_set_dst(sm, enter, child_state_ids.initial);
            bru_smir_set_dst(sm, leave, state_ids.final);

            out = bru_smir_add_transition(sm, child_state_ids.final);
            bru_smir_set_dst(sm, out, state_ids.final);
            break;

        /* TODO: */
        case BRU_COUNTER:   /* fallthrough */
        case BRU_LOOKAHEAD: /* fallthrough */
        case BRU_BACKREFERENCE: assert(0 && "TODO"); break;
        case BRU_NREGEXTYPES: assert(0 && "unreachable"); break;
    }

    return state_ids;
}
