#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "path_encoding.h"

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
        if (nout > 1)
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
