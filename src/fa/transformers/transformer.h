#ifndef BRU_FA_TRANSFORMER_H
#define BRU_FA_TRANSFORMER_H

#include <stc/fatp/slice.h>

#include "../smir.h"

typedef BruStateMachine *bru_transformer_f(BruStateMachine *);

typedef int bru_state_predicate_f(BruStateMachine *, bru_state_id);
typedef int bru_trans_predicate_f(BruStateMachine *, bru_trans_id);

typedef bru_transformer_f     transformer_f;
typedef bru_state_predicate_f state_predicate_f;
typedef bru_trans_predicate_f trans_predicate_f;

#if !defined(BRU_FA_TRANSFORMER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_TRANSFORMER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&            \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||         \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define transform_from_states      bru_transform_from_states
#    define transform_from_transitions bru_transform_from_transitions
#    define transform_with_states      bru_transform_with_states
#    define transform_with_trans       bru_transform_with_trans
#endif /* BRU_FA_TRANSFORMER_ENABLE_SHORT_NAMES */

/* TODO: revisit and make functions GREAT AGAIN */

/**
 * Create the submachine induced by a set of state identifiers.
 *
 * The set is indicated by a boolean array of length equal to the number of
 * states in the state machine. If a state identifier in the given state machine
 * maps to a non-zero value in the boolean vector, it will be in the new state
 * machine.
 *
 * Note that the old state identifiers will not necessarily map to corresponding
 * states in the transformed state machine.
 *
 * @param[in]     sm     the state machine
 * @param[in,out] states the boolean array indicating which states to keep,
 *                       i.e., the map of old state identifiers to new state
 *                       identifiers where 0 indicates the state does not
 *                       exist
 *
 * @return the submachine
 */
BruStateMachine *bru_transform_from_states(BruStateMachine *sm,
                                           bru_state_id    *states);

/**
 * Create the submachine induced by a set of transition identifiers.
 *
 * The set is indicated by a slice of transition identifiers to keep.
 * If a transition identifier in the given state machine is in the vector, then
 * both of its source and destination states will be in the transformed state
 * machine.
 *
 * Note that the order transitions appear in the vector dictates the order in
 * which they are added to the outgoing transitions of the induced source state.
 * As such, it is possible transitions will be added to the same state in a
 * different order to the original state machine.
 *
 * @param[in] sm          the state machine
 * @param[in] transitions the slice indicating which transitions to keep
 *
 * @return the submachine
 */
BruStateMachine *
bru_transform_from_transitions(BruStateMachine       *sm,
                               StcSlice(bru_trans_id) tranitions);

/**
 * Create the submachine induced by the states matching a predicate.
 *
 * If the predicate is NULL, the original state machine is returned.
 *
 * @param[in] sm  the state machine
 * @param[in] spf the predicate function over states
 *
 * @return the submachine (possibly the original)
 */
BruStateMachine *bru_transform_with_states(BruStateMachine       *sm,
                                           bru_state_predicate_f *spf);

/**
 * Create the submachine induced by the transitions matching a predicate.
 *
 * If the predicate is NULL, the original state machine is returned.
 *
 * @param[in] sm  the state machine
 * @param[in] tpf the predicate function over transitions
 *
 * @return the submachine (possibly the original)
 */
BruStateMachine *bru_transform_with_trans(BruStateMachine       *sm,
                                          bru_trans_predicate_f *tpf);

#endif /* BRU_FA_TRANSFORMER_H */
