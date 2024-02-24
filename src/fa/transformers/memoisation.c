#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "memoisation.h"

/* --- Helper Routines ------------------------------------------------------ */

static byte *memoise_in(StateMachine *sm)
{
    size_t    i, n, nstates = smir_get_num_states(sm);
    size_t   *in_degrees = calloc(nstates, sizeof(size_t));
    byte     *memo_sids  = malloc(sizeof(byte) * nstates);
    state_id  sid, dst;
    trans_id *out_transitions;

    for (sid = 1; sid <= nstates; sid++) {
        out_transitions = smir_get_out_transitions(sm, sid, &n);
        for (i = 0; i < n; i++) {
            dst = smir_get_dst(sm, out_transitions[i]);
            if (!IS_FINAL_STATE(dst)) in_degrees[dst - 1]++;
        }
        if (out_transitions) free(out_transitions);
    }

    for (sid = 1; sid <= nstates; sid++)
        memo_sids[sid - 1] = in_degrees[sid - 1] > 1;

    if (in_degrees) free(in_degrees);

    return memo_sids;
}

static void cn_dfs(StateMachine *sm,
                   state_id      sid,
                   byte         *has_backedge,
                   byte         *finished,
                   byte         *on_path)
{
    trans_id *out_transitions;
    size_t    i, n;
    state_id  dst;

    on_path[sid - 1] = TRUE;
    out_transitions  = smir_get_out_transitions(sm, sid, &n);
    for (i = 0; i < n; i++) {
        dst = smir_get_dst(sm, out_transitions[i]);

        if (IS_FINAL_STATE(dst) || finished[dst - 1]) continue;

        if (on_path[dst - 1]) {
            has_backedge[dst - 1] = TRUE;
            continue;
        }

        cn_dfs(sm, dst, has_backedge, finished, on_path);
    }

    if (out_transitions) free(out_transitions);
    on_path[sid - 1]  = FALSE;
    finished[sid - 1] = TRUE;
}

static byte *memoise_cn(StateMachine *sm)
{
    size_t nstates   = smir_get_num_states(sm);
    byte  *memo_sids = calloc(nstates, sizeof(byte));
    byte  *on_path   = calloc(nstates, sizeof(byte));
    byte  *finished  = calloc(nstates, sizeof(byte));

    cn_dfs(sm, 1, memo_sids, finished, on_path);

    free(on_path);
    free(finished);

    return memo_sids;
}

static void memoise_states(StateMachine *sm, byte *sids)
{
    state_id sid;
    size_t   k = SIZE_MAX;
    // NOTE: `k` can be any value -- it must only be unique amongst MEMO
    // actions. Of course, if there is already memoisation then there could be
    // overlap. It is expected that starting at the maximum possible value for k
    // and decrementing should give the least overlap.

    for (sid = 1; sid <= smir_get_num_states(sm); sid++)
        if (sids[sid - 1])
            smir_state_prepend_action(sm, sid, smir_action_num(ACT_MEMO, k--));
}

/* --- Main Routine --------------------------------------------------------- */

StateMachine *transform_memoise(StateMachine *sm, MemoScheme memo)
{
    byte *memo_sids;

    if (!sm) return sm;

    switch (memo) {
        case MS_NONE: return sm;
        case MS_IN: memo_sids = memoise_in(sm); break;
        case MS_CN: memo_sids = memoise_cn(sm); break;
        case MS_IAR: return sm;
    }

    memoise_states(sm, memo_sids);
    if (memo_sids) free(memo_sids);

    return sm;
}
