#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../utils.h"
#include "path_encoding.h"

/* --- Helper functions ----------------------------------------------------- */

static bru_byte_t *memoise_in(BruStateMachine *sm)
{
    size_t        i, n, nstates = bru_smir_get_num_states(sm);
    size_t       *in_degrees = calloc(nstates, sizeof(size_t));
    bru_byte_t   *memo_sids  = calloc(nstates, sizeof(bru_byte_t));
    bru_state_id  sid, dst;
    bru_trans_id *out_transitions;

    for (sid = 0; sid <= nstates; sid++) {
        out_transitions = bru_smir_get_out_transitions(sm, sid, &n);
        for (i = 0; i < n; i++) {
            dst = bru_smir_get_dst(sm, out_transitions[i]);
            if (!BRU_IS_FINAL_STATE(dst)) in_degrees[dst - 1]++;
        }
        if (out_transitions) free(out_transitions);
    }

    for (sid = 1; sid <= nstates; sid++)
        memo_sids[sid - 1] = in_degrees[sid - 1] > 1;

    if (in_degrees) free(in_degrees);

    return memo_sids;
}

static void cn_dfs(BruStateMachine *sm,
                   bru_state_id     sid,
                   bru_byte_t      *has_backedge,
                   bru_byte_t      *on_path)
{
    bru_trans_id *out_transitions;
    size_t        i, n;
    bru_state_id  dst;

    if (sid) on_path[sid - 1] = TRUE;
    out_transitions = bru_smir_get_out_transitions(sm, sid, &n);
    for (i = 0; i < n; i++) {
        dst = bru_smir_get_dst(sm, out_transitions[i]);

        if (BRU_IS_FINAL_STATE(dst)) continue;

        if (on_path[dst - 1]) {
            has_backedge[dst - 1] = TRUE;
            continue;
        }

        cn_dfs(sm, dst, has_backedge, on_path);
    }

    if (out_transitions) free(out_transitions);
    if (sid) on_path[sid - 1] = FALSE;
}

static bru_byte_t *memoise_cn(BruStateMachine *sm)
{
    size_t      nstates   = bru_smir_get_num_states(sm);
    bru_byte_t *memo_sids = calloc(nstates, sizeof(bru_byte_t));
    bru_byte_t *on_path;

    if (nstates > 0) {
        on_path = calloc(nstates, sizeof(bru_byte_t));
        cn_dfs(sm, BRU_INITIAL_STATE_ID, memo_sids, on_path);
        free(on_path);
    }

    return memo_sids;
}

/**
 * Memoise the states of a state machine in the given set of state identifiers.
 *
 * The set of state identifiers, `sids`, is interpretted as a boolean array
 * indexed by state identifier. A non-zero value in the array for a given state
 * identifier `i` indicates that state should be memoised, while a value of zero
 * indicates it should not be.
 *
 * @param[in] sm      the state machine
 * @param[in] sids    a boolean array indicating if a state is in the set to be
 *                    memoised
 * @param[in] logfile the file for logging output
 */
static void memoise_states(BruStateMachine *sm, bru_byte_t *sids, FILE *logfile)
{
    bru_state_id sid;
    size_t       nstates, k = SIZE_MAX;
    // NOTE: `k` can be any value -- it must only be unique amongst MEMO
    // actions. Of course, if there is already memoisation then there could be
    // overlap. It is expected that starting at the maximum possible value for k
    // and decrementing should give the least overlap.

    for (sid = 1, nstates = bru_smir_get_num_states(sm); sid <= nstates; sid++)
        if (sids[sid - 1])
            bru_smir_state_prepend_action(
                sm, sid, bru_smir_action_num(BRU_ACT_MEMO, k--));

#ifdef BRU_BENCHMARK
    if (logfile)
        fprintf(logfile, "NUMBER OF STATES MEMOISED: %zu\n", SIZE_MAX - k);
#else
    BRU_UNUSED(logfile);
#endif /* BRU_BENCHMARK */
}

/* --- API function definitions --------------------------------------------- */

BruStateMachine *bru_transform_path_encode(BruStateMachine *sm)
{
    bru_state_id  sid;
    bru_trans_id *out_transitions;
    size_t        nout, nstates, j, rem, quot;

    if (!sm) return sm;

    for (sid = BRU_INITIAL_STATE_ID, nstates = bru_smir_get_num_states(sm);
         sid < nstates; sid++) {
        out_transitions = bru_smir_get_out_transitions(sm, sid, &nout);
        if (nout > 2)
            for (j = 0; j < nout; j++) {
                bru_smir_trans_prepend_action(
                    sm, out_transitions[j],
                    bru_smir_action_num(BRU_ACT_WRITE, ' '));
                quot = j;
                do {
                    rem  = quot % 10;
                    quot = quot / 10;
                    bru_smir_trans_prepend_action(
                        sm, out_transitions[j],
                        bru_smir_action_num(BRU_ACT_WRITE, '0' + rem));
                } while (quot);
            }
        free(out_transitions);
    }

    return sm;
}
