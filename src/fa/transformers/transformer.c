#include <stdlib.h>

#include <stc/fatp/vec.h>

#include "transformer.h"

/* --- API function definitions --------------------------------------------- */

BruStateMachine *bru_transform_from_states(BruStateMachine *old_sm,
                                           bru_state_id    *states)
{
    BruStateMachine *new_sm;
    bru_trans_id    *out_transitions, old_tid, new_tid;
    bru_state_id     old_sid, new_src, new_dst, old_dst;
    size_t           n, i;

    new_sm = bru_smir_default(bru_smir_get_regex(old_sm));

    // build map from old states to new
    for (old_sid = 1; old_sid <= bru_smir_get_num_states(old_sm); old_sid++) {
        if (!states[old_sid - 1]) continue;
        states[old_sid - 1] = bru_smir_add_state(new_sm);

        // add state actions
        if (bru_smir_state_get_num_actions(old_sm, old_sid))
            bru_smir_state_set_actions(
                new_sm, states[old_sid - 1],
                bru_smir_state_clone_actions(old_sm, old_sid));
    }

    // add initial transitions
    out_transitions = bru_smir_get_initial(old_sm, &n);
    for (i = 0; i < n; i++) {
        old_tid = out_transitions[i];

        if ((new_dst = states[bru_smir_get_dst(old_sm, old_tid) - 1])) {
            new_tid = bru_smir_set_initial(new_sm, new_dst);
            if (bru_smir_trans_get_num_actions(old_sm, old_tid)) {
                bru_smir_trans_set_actions(
                    new_sm, new_tid,
                    bru_smir_trans_clone_actions(old_sm, old_tid));
            }
        }
    }
    if (out_transitions) free(out_transitions);

    // add state transitions
    for (old_sid = 1; old_sid <= bru_smir_get_num_states(old_sm); old_sid++) {
        if (!(new_src = states[old_sid - 1])) continue;
        out_transitions = bru_smir_get_out_transitions(old_sm, old_sid, &n);

        for (i = 0; i < n; i++) {
            old_tid = out_transitions[i];

            old_dst = bru_smir_get_dst(old_sm, old_tid);

            if (BRU_IS_FINAL_STATE(old_dst)) {
                new_tid = bru_smir_set_final(new_sm, new_src);
            } else if ((new_dst = states[old_dst - 1])) {
                new_tid = bru_smir_add_transition(new_sm, new_src);
                bru_smir_set_dst(new_sm, new_tid, new_dst);
            } else {
                continue;
            }

            // add transition actions
            if (bru_smir_trans_get_num_actions(old_sm, old_tid)) {
                bru_smir_state_set_actions(
                    new_sm, new_src,
                    bru_smir_trans_clone_actions(old_sm, old_tid));
            }
        }

        if (out_transitions) free(out_transitions);
    }

    return new_sm;
}

BruStateMachine *
bru_transform_from_transitions(BruStateMachine       *old_sm,
                               StcSlice(bru_trans_id) transitions)
{
    BruStateMachine *new_sm;
    bru_state_id    *states;
    bru_trans_id     old_tid, new_tid;
    bru_state_id     src, dst;
    size_t           i;

    // TODO: handle initial and final transitions

    new_sm = bru_smir_default(bru_smir_get_regex(old_sm));

    if (!stc_slice_len_unsafe(transitions)) return new_sm;

    states = calloc(bru_smir_get_num_states(old_sm), sizeof(*states));
    for (i = 0; i < stc_slice_len_unsafe(transitions); i++) {
        old_tid = transitions[i];

        // for each transition, compute the new state identifiers
        src = bru_smir_get_src(old_sm, old_tid);
        dst = bru_smir_get_dst(old_sm, old_tid);

        if (!BRU_IS_INITIAL_STATE(src) && !states[src - 1]) {
            states[src - 1] = bru_smir_add_state(new_sm);
            if (bru_smir_state_get_num_actions(old_sm, src)) {
                bru_smir_state_set_actions(
                    new_sm, states[src - 1],
                    bru_smir_state_clone_actions(old_sm, src));
            }
        }

        if (!BRU_IS_FINAL_STATE(dst) && !states[dst - 1]) {
            states[dst - 1] = bru_smir_add_state(new_sm);
            if (bru_smir_state_get_num_actions(old_sm, dst)) {
                bru_smir_state_set_actions(
                    new_sm, states[dst - 1],
                    bru_smir_state_clone_actions(old_sm, dst));
            }
        }

        // insert transition
        if (BRU_IS_INITIAL_STATE(src)) {
            new_tid = bru_smir_set_initial(new_sm, states[dst - 1]);
        } else if (BRU_IS_FINAL_STATE(dst)) {
            new_tid = bru_smir_set_final(new_sm, states[src - 1]);
        } else {
            new_tid = bru_smir_add_transition(new_sm, states[src - 1]);
            bru_smir_set_dst(new_sm, new_tid, states[dst - 1]);
        }

        // copy over actions
        if (bru_smir_trans_get_num_actions(old_sm, old_tid)) {
            bru_smir_trans_set_actions(
                new_sm, new_tid, bru_smir_trans_clone_actions(old_sm, old_tid));
        }
    }

    if (states) free(states);
    return new_sm;
}

BruStateMachine *bru_transform_with_states(BruStateMachine       *sm,
                                           bru_state_predicate_f *spf)
{
    unsigned int *states;
    bru_state_id  sid;

    if (!(spf && sm)) return sm;

    states = malloc(sizeof(*states) * bru_smir_get_num_states(sm));
    for (sid = 1; sid <= bru_smir_get_num_states(sm); sid++)
        states[sid - 1] = spf(sm, sid);

    sm = bru_transform_from_states(sm, states);
    free(states);
    return sm;
}

BruStateMachine *bru_transform_with_trans(BruStateMachine       *sm,
                                          bru_trans_predicate_f *tpf)
{
    StcVec(bru_trans_id) transitions;
    bru_trans_id        *out_trans;
    bru_state_id         sid;
    size_t               n, i;

    if (!(tpf && sm)) return sm;

    stc_vec_default_init(transitions);

    for (sid = 1; sid <= bru_smir_get_num_states(sm); sid++) {
        out_trans = bru_smir_get_out_transitions(sm, sid, &n);
        for (i = 0; i < n; i++)
            if (tpf(sm, out_trans[i]))
                stc_vec_push_back(transitions, out_trans[i]);
        if (out_trans) free(out_trans);
    }

    sm = bru_transform_from_transitions(sm, transitions);
    stc_vec_free(transitions);
    return sm;
}
