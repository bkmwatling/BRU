#include <stdlib.h>

#include "../stc/fatp/vec.h"

#include "memoisation.h"
#include "transformer.h"

/* --- Helper Routines ------------------------------------------------------ */

static byte *memoise_in(StateMachine *sm)
{
    size_t    i, n, nstates = smir_get_num_states(sm);
    size_t   *in_degrees = calloc(nstates, sizeof(unsigned int));
    byte     *memo_sids  = malloc(sizeof(byte) * nstates);
    state_id  sid, dst;
    trans_id *out_transitions, tid;

    for (sid = 1; sid <= nstates; sid++) {
        out_transitions = smir_get_out_transitions(sm, sid, &n);
        for (i = 0; i < n; i++) {
            dst = out_transitions[i];
            if (!IS_FINAL_STATE(dst)) in_degrees[dst - 1]++;
        }
        if (out_transitions) free(out_transitions);
    }

    for (sid = 1; sid <= nstates; sid++)
        if (in_degrees[sid - 1] > 1) memo_sids[sid - 1] = TRUE;

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
        dst = out_transitions[i];

        if (IS_FINAL_STATE(dst) || finished[dst - 1]) continue;

        if (on_path[dst - 1]) {
            has_backedge[dst - 1] = TRUE;
            continue;
        }

        cn_dfs(sm, dst, has_backedge, finished, on_path);
    }

    on_path[sid - 1]  = FALSE;
    finished[sid - 1] = TRUE;
}

static byte *memoise_cn(StateMachine *sm)
{
    size_t i, n, nstates = smir_get_num_states(sm);
    byte  *memo_sids = calloc(nstates, sizeof(byte));
    byte  *on_path   = calloc(nstates, sizeof(byte));
    byte  *finished  = calloc(nstates, sizeof(byte));

    cn_dfs(sm, 1, memo_sids, finished, on_path);

    return memo_sids;
}

static void memoise_states(StateMachine *sm, byte *sids)
{
    // TODO: rework the meaning of `k` to be a unique identifier that relates
    // instructions to their position in memory
    state_id sid;
    len_t    k = 0;

    for (sid = 1; sid <= smir_get_num_states(sm); sid++) {
        if (sids[sid - 1]) {
            smir_state_prepend_action(sm, sid, smir_action_num(ACT_MEMO, k));
            k += sizeof(byte *);
        }
    }
}

/* --- Main Routine --------------------------------------------------------- */

StateMachine *transform_memoise(StateMachine *sm, MemoScheme memo)
{
    byte *memo_sids;
    len_t k;

    if (!sm) return sm;

    switch (memo) {
        case MS_IN: memo_sids = memoise_in(sm); break;
        case MS_CN: memo_sids = memoise_cn(sm); break;
        case MS_IAR: return sm;
    }

    memoise_states(sm, memo_sids);
    if (memo_sids) free(memo_sids);

    return sm;
}
