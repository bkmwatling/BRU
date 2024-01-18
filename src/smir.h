#ifndef _SMIR_H
#define _SMIR_H

#include <cstddef>
#include <stdint.h>

/* --- Data Structures ----------------------------------------------------- */

typedef struct predicate     Predicate; // regex node?
typedef Predicate            Action;
typedef struct state_machine StateMachine;

typedef uint32_t state_id; // 0 => nonexistent
typedef uint64_t trans_id; // (src state_id, idx into outgoing transitions)

/* --- API ----------------------------------------------------------------- */

/**
 * Create a blank state machine.
 *
 * @return the blank state machine
 */
StateMachine smir_default(void);

/**
 * Create a new state machine with given number of states.
 *
 * Each state is assigned a unique state identifier in [1...nstates].
 *
 * @param[in] nstates the number of states
 *
 * @return the new state machine
 */
StateMachine smir_new(uint32_t nstates);

/**
 * Create a new state in the state machine.
 *
 * @param[in] self the state machine
 *
 * @return the unique state identifier
 */
state_id smir_add_state(StateMachine *self);

/**
 * Mark a state as initial.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 */
void smir_set_initial(StateMachine *self, state_id sid);

/**
 * Mark a state as final.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 */
void smir_set_final(StateMachine *self, state_id sid);

/**
 * Add an outgoing transition to a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the unique transition identifer
 */
trans_id smir_add_transition(StateMachine *self, state_id sid);

/**
 * Get the outgoing transitions of a state.
 *
 * The returned array is created with a call to `malloc`, and the memory
 * should be released using a single call to `free`.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[out] n   the length of the returned array
 *
 * @return an array of transition identifiers representing the outgoing
 *         transitions
 */
trans_id *
smir_get_outgoing_transitions(StateMachine *self, state_id sid, size_t *n);

/**
 * Get the incoming transitions of a state.
 *
 * The returned array is created with a call to `malloc`, and the memory
 * should be released using a single call to `free`.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[out] n   the length of the returned array
 *
 * @return an array of transition identifiers representing the incoming
 *         transitions
 */
trans_id *
smir_get_incoming_transitions(StateMachine *self, state_id sid, size_t *n);

/**
 * Get a state's predicate.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the predicate, where NULL indicates the state has no predicate.
 */
Predicate *smir_get_predicate(StateMachine *self, state_id sid);

/**
 * Set a state's predicate.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] pred the predicate
 */
void smir_set_predicate(StateMachine *self, state_id sid, Predicate *pred);

/**
 * Set the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] dest the unique destination state identifier
 */
void smir_set_dest(StateMachine *self, trans_id tid, state_id dest);

/**
 * Get the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique destination state identifier
 */
state_id smir_get_dest(StateMachine *self, trans_id tid);

/**
 * Get the source state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique source state identifier
 */
state_id smir_get_src(StateMachine *self, trans_id tid);

/**
 * Get the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the actions of the transition
 */
Action *smir_get_actions(StateMachine *self, trans_id tid);

/**
 * Append an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action
 */
void smir_append_action(StateMachine *self, trans_id tid, Action *act);

/**
 * Set the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] acts the list of actions
 */
void smir_set_actions(StateMachine *self, trans_id tid, Action *acts);

/* --- Extendable API ------------------------------------------------------ */

/**
 * Set the pre-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] meta the pre-predicate meta data
 */
void smir_set_pre_meta(StateMachine *self, state_id sid, void *meta);

/**
 * Get the pre-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the pre-predicate meta data
 */
void *smir_get_pre_meta(StateMachine *self, state_id sid);

/**
 * Set the post-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] meta the post-predicate meta data
 */
void smir_set_post_meta(StateMachine *self, state_id sid, void *meta);

/**
 * Get the post-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the post-predicate meta data
 */
void *smir_get_post_meta(StateMachine *self, state_id sid);

#endif
