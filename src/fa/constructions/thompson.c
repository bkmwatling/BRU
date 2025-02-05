#include <assert.h>

#include <bru/fa/constructions/thompson.h>
#include <bru/re/sre.h>

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
} BruStateMachineFragment;

/* --- Helper function prototypes ------------------------------------------- */

static BruStateMachineFragment
emit(BruStateMachine *sm, const BruRegexNode *re, BruConstructionOpts opts);

/* --- API function definitions --------------------------------------------- */

BruStateMachine *bru_thompson_construct(BruRegex re, BruConstructionOpts opts)
{
    BruStateMachine        *sm;
    BruStateMachineFragment frag;

    sm   = bru_smir_default(re.regex);
    frag = emit(sm, re.root, opts);
    bru_smir_set_initial(sm, frag.initial);
    bru_smir_set_final(sm, frag.final);

    return sm;
}

/* --- Helper functions ----------------------------------------------------- */

static BruStateMachineFragment
emit(BruStateMachine *sm, const BruRegexNode *re, BruConstructionOpts opts)
{
    BruStateMachineFragment frag, child_frag;
    bru_trans_id            out, enter, leave;
    bru_state_id            start, end;

    switch (re->type) {
        case BRU_EPSILON:
            frag.initial = frag.final = bru_smir_add_state(sm);
            break;

        case BRU_CARET:
            frag.initial = frag.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(sm, frag.final,
                                         bru_smir_action_zwa(BRU_ACT_BEGIN));
            break;

        case BRU_DOLLAR:
            frag.initial = frag.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(sm, frag.final,
                                         bru_smir_action_zwa(BRU_ACT_END));
            break;

        // case MEMOISE:
        //     state_ids.initial = state_ids.final = smir_add_state(sm);
        //     smir_state_append_action(sm, state_ids.final,
        //                              smir_action_num(BRU_ACT_MEMO, re->rid));
        //     break;
        //
        case BRU_LITERAL:
            frag.initial = frag.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(sm, frag.final,
                                         bru_smir_action_char(re->ch));
            break;

        case BRU_CC:
            frag.initial = frag.final = bru_smir_add_state(sm);
            bru_smir_state_append_action(
                sm, frag.final,
                bru_smir_action_predicate(bru_intervals_clone(re->intervals)));
            break;

        case BRU_ALT:
            frag.initial = bru_smir_add_state(sm);

            child_frag = emit(sm, re->left, opts);
            out        = bru_smir_add_transition(sm, frag.initial);
            bru_smir_set_dst(sm, out, child_frag.initial);
            out = bru_smir_add_transition(sm, child_frag.final);

            child_frag = emit(sm, re->right, opts);
            leave      = bru_smir_add_transition(sm, frag.initial);
            bru_smir_set_dst(sm, leave, child_frag.initial);
            leave = bru_smir_add_transition(sm, child_frag.final);

            frag.final = bru_smir_add_state(sm);
            bru_smir_set_dst(sm, out, frag.final);
            bru_smir_set_dst(sm, leave, frag.final);
            break;

        case BRU_CONCAT:
            frag       = emit(sm, re->left, opts);
            child_frag = emit(sm, re->right, opts);

            out = bru_smir_add_transition(sm, frag.final);
            bru_smir_set_dst(sm, out, child_frag.initial);

            frag.final = child_frag.final;
            break;

        case BRU_CAPTURE:
            frag.initial = bru_smir_add_state(sm);
            child_frag   = emit(sm, re->left, opts);
            frag.final   = bru_smir_add_state(sm);

            out = bru_smir_add_transition(sm, frag.initial);
            bru_smir_set_dst(sm, out, child_frag.initial);
            bru_smir_trans_append_action(
                sm, out,
                bru_smir_action_num(BRU_ACT_SAVE, 2 * re->capture_idx));

            out = bru_smir_add_transition(sm, child_frag.final);
            bru_smir_set_dst(sm, out, frag.final);
            bru_smir_trans_append_action(
                sm, out,
                bru_smir_action_num(BRU_ACT_SAVE, 2 * re->capture_idx + 1));
            break;

        case BRU_STAR:
            switch (opts.capture_semantics) {
                case BRU_CS_PCRE:
                    frag.initial = bru_smir_add_state(sm);
                    start        = bru_smir_add_state(sm);
                    child_frag   = emit(sm, re->left, opts);
                    frag.final   = bru_smir_add_state(sm);

                    SET_TRANS_PRIORITY(sm, re, frag.initial, enter, leave);
                    bru_smir_set_dst(sm, enter, start);
                    bru_smir_set_dst(sm, leave, frag.final);

                    enter = bru_smir_add_transition(sm, start);
                    bru_smir_set_dst(sm, enter, child_frag.initial);
                    bru_smir_trans_append_action(
                        sm, enter,
                        bru_smir_action_num(BRU_ACT_EPSSET, re->rid));

                    SET_TRANS_PRIORITY(sm, re, child_frag.final, enter, leave);
                    bru_smir_set_dst(sm, enter, start);
                    bru_smir_set_dst(sm, leave, frag.final);
                    bru_smir_trans_append_action(
                        sm, enter,
                        bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));
                    break;

                case BRU_CS_RE2:
                    frag.initial = bru_smir_add_state(sm);
                    child_frag   = emit(sm, re->left, opts);
                    end          = bru_smir_add_state(sm);
                    frag.final   = bru_smir_add_state(sm);

                    SET_TRANS_PRIORITY(sm, re, frag.initial, enter, leave);
                    bru_smir_set_dst(sm, enter, child_frag.initial);
                    bru_smir_set_dst(sm, leave, frag.final);

                    out = bru_smir_add_transition(sm, child_frag.final);
                    bru_smir_set_dst(sm, out, end);
                    bru_smir_trans_append_action(
                        sm, out, bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));

                    SET_TRANS_PRIORITY(sm, re, end, enter, leave);
                    bru_smir_set_dst(sm, enter, child_frag.initial);
                    bru_smir_set_dst(sm, leave, frag.final);
                    bru_smir_trans_append_action(
                        sm, enter,
                        bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
                    break;
            }
            break;

        case BRU_PLUS:
            switch (opts.capture_semantics) {
                case BRU_CS_PCRE:
                    frag.initial = bru_smir_add_state(sm);
                    child_frag   = emit(sm, re->left, opts);
                    frag.final   = bru_smir_add_state(sm);

                    out = bru_smir_add_transition(sm, frag.initial);
                    bru_smir_set_dst(sm, out, child_frag.initial);
                    bru_smir_trans_append_action(
                        sm, out, bru_smir_action_num(BRU_ACT_EPSSET, re->rid));

                    SET_TRANS_PRIORITY(sm, re, child_frag.final, enter, leave);
                    bru_smir_set_dst(sm, enter, frag.initial);
                    bru_smir_set_dst(sm, leave, frag.final);
                    bru_smir_trans_append_action(
                        sm, enter,
                        bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));
                    break;

                case BRU_CS_RE2:
                    frag = child_frag = emit(sm, re->left, opts);
                    end               = bru_smir_add_state(sm);
                    frag.final        = bru_smir_add_state(sm);

                    out = bru_smir_add_transition(sm, child_frag.final);
                    bru_smir_set_dst(sm, out, end);
                    bru_smir_trans_append_action(
                        sm, out, bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));

                    SET_TRANS_PRIORITY(sm, re, end, enter, leave);
                    bru_smir_set_dst(sm, enter, frag.initial);
                    bru_smir_set_dst(sm, leave, frag.final);
                    bru_smir_trans_append_action(
                        sm, enter,
                        bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
                    break;
            }
            break;

        case BRU_QUES:
            frag.initial = bru_smir_add_state(sm);
            child_frag   = emit(sm, re->left, opts);
            frag.final   = bru_smir_add_state(sm);

            SET_TRANS_PRIORITY(sm, re, frag.initial, enter, leave);
            bru_smir_set_dst(sm, enter, child_frag.initial);
            bru_smir_set_dst(sm, leave, frag.final);

            out = bru_smir_add_transition(sm, child_frag.final);
            bru_smir_set_dst(sm, out, frag.final);
            break;

        case BRU_COUNTER:
            switch (opts.capture_semantics) {
                case BRU_CS_PCRE:
                    frag.initial = bru_smir_add_state(sm);
                    start =
                        re->min == 0 ? bru_smir_add_state(sm) : frag.initial;
                    if (re->max == BRU_CNTR_MAX) end = bru_smir_add_state(sm);
                    child_frag = emit(sm, re->left, opts);
                    frag.final = bru_smir_add_state(sm);

                    if (re->min == 0) {
                        SET_TRANS_PRIORITY(sm, re, frag.initial, enter, leave);
                        bru_smir_set_dst(sm, enter, start);
                        bru_smir_set_dst(sm, leave, frag.final);
                    }

                    if (re->max < BRU_CNTR_MAX) {
                        out = bru_smir_add_transition(sm, start);
                        bru_smir_set_dst(sm, out, child_frag.initial);
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_num(BRU_ACT_INC, re->counter_idx));
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
                    } else {
                        out = bru_smir_add_transition(sm, start);
                        bru_smir_set_dst(sm, out, end);
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_cmp(re->counter_idx, re->min,
                                                BRU_LT));
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_num(BRU_ACT_INC, re->counter_idx));

                        out = bru_smir_add_transition(sm, start);
                        bru_smir_set_dst(sm, out, end);
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_cmp(re->counter_idx, re->min,
                                                BRU_GE));

                        out = bru_smir_add_transition(sm, end);
                        bru_smir_set_dst(sm, out, child_frag.initial);
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_num(BRU_ACT_EPSSET, re->rid));
                    }

                    SET_TRANS_PRIORITY(sm, re, child_frag.final, enter, leave);
                    bru_smir_set_dst(sm, enter, start);
                    bru_smir_set_dst(sm, leave, frag.final);
                    if (re->max < BRU_CNTR_MAX)
                        bru_smir_trans_append_action(
                            sm, enter,
                            bru_smir_action_cmp(re->counter_idx, re->max,
                                                BRU_LT));
                    bru_smir_trans_append_action(
                        sm, enter,
                        bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));

                    if (!re->left->nullable && re->min)
                        bru_smir_trans_append_action(
                            sm, leave,
                            bru_smir_action_cmp(re->counter_idx, re->min,
                                                BRU_GE));
                    bru_smir_trans_append_action(
                        sm, leave, bru_smir_action_set(re->counter_idx, 0));
                    break;

                case BRU_CS_RE2:
                    frag.initial = bru_smir_add_state(sm);
                    start =
                        re->min == 0 ? bru_smir_add_state(sm) : frag.initial;
                    child_frag = emit(sm, re->left, opts);
                    end        = bru_smir_add_state(sm);
                    frag.final = bru_smir_add_state(sm);

                    if (re->min == 0) {
                        SET_TRANS_PRIORITY(sm, re, frag.initial, enter, leave);
                        bru_smir_set_dst(sm, enter, start);
                        bru_smir_set_dst(sm, leave, frag.final);
                    }

                    if (re->max < BRU_CNTR_MAX) {
                        out = bru_smir_add_transition(sm, start);
                        bru_smir_set_dst(sm, out, child_frag.initial);
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_num(BRU_ACT_INC, re->counter_idx));
                    } else {
                        out = bru_smir_add_transition(sm, start);
                        bru_smir_set_dst(sm, out, child_frag.initial);
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_cmp(re->counter_idx, re->min,
                                                BRU_LT));
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_num(BRU_ACT_INC, re->counter_idx));

                        out = bru_smir_add_transition(sm, start);
                        bru_smir_set_dst(sm, out, child_frag.initial);
                        bru_smir_trans_append_action(
                            sm, out,
                            bru_smir_action_cmp(re->counter_idx, re->min,
                                                BRU_GE));
                    }

                    out = bru_smir_add_transition(sm, child_frag.final);
                    bru_smir_set_dst(sm, out, end);
                    bru_smir_trans_append_action(
                        sm, out, bru_smir_action_num(BRU_ACT_EPSCHK, re->rid));

                    SET_TRANS_PRIORITY(sm, re, end, enter, leave);
                    bru_smir_set_dst(sm, enter, start);
                    bru_smir_set_dst(sm, leave, frag.final);
                    if (re->max < BRU_CNTR_MAX)
                        bru_smir_trans_append_action(
                            sm, enter,
                            bru_smir_action_cmp(re->counter_idx, re->max,
                                                BRU_LT));
                    bru_smir_trans_append_action(
                        sm, enter,
                        bru_smir_action_num(BRU_ACT_EPSSET, re->rid));

                    if (!re->left->nullable && re->min)
                        bru_smir_trans_append_action(
                            sm, leave,
                            bru_smir_action_cmp(re->counter_idx, re->min,
                                                BRU_GE));
                    bru_smir_trans_append_action(
                        sm, leave, bru_smir_action_set(re->counter_idx, 0));
                    break;
            }
            break;

        /* TODO: */
        case BRU_LOOKAHEAD: /* fallthrough */
        case BRU_BACKREFERENCE: assert(0 && "TODO"); break;
        case BRU_NREGEXTYPES: assert(0 && "unreachable"); break;
    }

    return frag;
}
