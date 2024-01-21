#ifndef SMIR_H
#define SMIR_H

#include <stddef.h>
#include <stdint.h>

#include "srvm.h"

/* --- Macros --------------------------------------------------------------- */

#define NULL_STATE       0
#define INITIAL_STATE_ID NULL_STATE
#define FINAL_STATE_ID   NULL_STATE

#define IS_INITIAL_STATE(sid) (sid == INITIAL_STATE_ID)
#define IS_FINAL_STATE(sid)   (sid == FINAL_STATE_ID)

/* --- Type Definitions ----------------------------------------------------- */

typedef Regex                Predicate;
typedef Regex                Action;
typedef struct action_list   ActionList;
typedef struct state_machine StateMachine;

typedef uint32_t state_id; // 0 => nonexistent
typedef uint64_t trans_id; // (src state_id, idx into outgoing transitions)

/**
 * TODO: decide on compilation function signature
 * e.g. void compile_f(void *pre_meta, Prog *p) => push instructions onto p
 *      void compile_f(void *pre_meta, byte *b) => push instructions onto b
 *      Prog *compile_f(void *pre_meta)         => compile into sub-program
 *      ...
 */
typedef void (*compile_f)(void *, Program *);

/**
 * TODO: decide on transformer functions for running graph algorithms
 * on the state machine.
 */
typedef void (*transform_f)(StateMachine *self);

/* --- API ------------------------------------------------------------------ */

/**
 * Create a blank state machine.
 *
 * @return the blank state machine
 */
StateMachine *smir_default(void);

/**
 * Create a new state machine with a given number of states.
 *
 * Each state is assigned a unique state identifier in [1...nstates].
 *
 * @param[in] nstates the number of states
 *
 * @return the new state machine
 */
StateMachine *smir_new(uint32_t nstates);

/**
 * Free all memory used by the state machine.
 *
 * @param[in] self the state machine
 */
void smir_free(StateMachine *self);

/**
 * Compile a state machine.
 *
 * @param[in] self the state machine
 *
 * @return the compiled program
 */
Program *smir_compile(StateMachine *self);

/**
 * Create a new state in the state machine.
 *
 * @param[in] self the state machine
 *
 * @return the unique state identifier
 */
state_id smir_add_state(StateMachine *self);

/**
 * Get the current number of states in the state machine.
 *
 * @param self the state machine
 *
 * @return the number of states
 */
size_t smir_get_nstates(StateMachine *self);

/**
 * Mark a state as initial by inserting a blank initial transition to the state.
 *
 * Note: The order of calls to this function determines the priorities by which
 * the initialisation functions are executed.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the unique transition identifier of the initialisation transition
 */
trans_id smir_set_initial(StateMachine *self, state_id sid);

/**
 * Get the initial transitions of the state machine.
 *
 * Convenience function equivalent to calling @ref smir_get_out_transitions
 * with @ref INITIAL_STATE_ID.
 *
 * @param[in]  self the state machine
 * @param[out] n    the number of initial transitions
 *
 * @return the identifiers representing the initial transitions
 */
trans_id *smir_get_initial(StateMachine *self, size_t *n);

/**
 * Mark a state as final.
 *
 * Note: When this function is called, the priority of the finalisation function
 * for the state will be lower than any transition added to the state before the
 * call.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the unique transition identifier of the finalisation transition
 */
trans_id smir_set_final(StateMachine *self, state_id sid);

/**
 * Add an outgoing transition to a state.
 *
 * The added transition will not have a destination state.
 * See \ref smir_set_dst.
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
trans_id *smir_get_out_transitions(StateMachine *self, state_id sid, size_t *n);

/**
 * Get a state's predicate.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the predicate, where NULL indicates the state has no predicate.
 */
const Predicate *smir_get_predicate(StateMachine *self, state_id sid);

/**
 * Set a state's predicate.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] pred the predicate
 */
void smir_set_predicate(StateMachine    *self,
                        state_id         sid,
                        const Predicate *pred);

/**
 * Get the source state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique state identifier of the source state
 */
state_id smir_get_src(StateMachine *self, trans_id tid);

/**
 * Get the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique state identifier of the destination state
 */
state_id smir_get_dst(StateMachine *self, trans_id tid);

/**
 * Set the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] dst  the unique state identifier of the destination state
 */
void smir_set_dst(StateMachine *self, trans_id tid, state_id dst);

/**
 * Get the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the actions of the transition
 */
const ActionList *smir_get_actions(StateMachine *self, trans_id tid);

/**
 * Append an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to append
 */
void smir_append_action(StateMachine *self, trans_id tid, const Action *act);

/**
 * Prepend an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to prepend
 */
void smir_prepend_action(StateMachine *self, trans_id tid, const Action *act);

/**
 * Set the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] acts the list of actions
 */
void smir_set_actions(StateMachine *self, trans_id tid, ActionList *acts);

/**
 * Create an empty list of actions.
 *
 * @return the empty list of actions
 */
ActionList *smir_action_list_new(void);

/**
 * Free the list of actions.
 *
 * @param[in] self the list of actions
 */
void smir_action_list_free(ActionList *self);

/**
 * Append an action to the list of actions.
 *
 * @param[in] self the list of actions
 * @param[in] act  the action to append
 */
void smir_action_list_append(ActionList *self, const Action *act);

/**
 * Prepend an action to the list of actions.
 *
 * @param[in] self the list of actions
 * @param[in] act  the action to prepend
 */
void smir_action_list_prepend(ActionList *self, const Action *act);

/* --- Extendable API ------------------------------------------------------- */

/**
 * Set the pre-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] meta the pre-predicate meta data
 *
 * @return the previous pre-predicate meta data (initially NULL)
 */
void *smir_set_pre_meta(StateMachine *self, state_id sid, void *meta);

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
 *
 * @return the previous post-predicate meta data (initially NULL)
 */
void *smir_set_post_meta(StateMachine *self, state_id sid, void *meta);

/**
 * Get the post-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the post-predicate meta data
 */
void *smir_get_post_meta(StateMachine *self, state_id sid);

/**
 * Compile the state machine to VM instructions, including the meta data.
 *
 * @param[in] self      the state machine
 * @param[in] pre_meta  the compiler for pre-predicate meta data at states
 * @param[in] post_meta the compiler for post-predicate meta data at states
 *
 * @return the compiled program
 */
Program *smir_compile_with_meta(StateMachine *self,
                                compile_f     pre_meta,
                                compile_f     post_meta);

/**
 * Transform the state machine (in-place) with the provided transformer.
 *
 * @param[in] self        the state machine
 * @param[in] transformer the transformer
 */
void smir_transform(StateMachine *self, transform_f transformer);

#endif /* SMIR_H */
