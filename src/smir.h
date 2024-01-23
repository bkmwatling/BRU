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

typedef enum {
    ACT_BEGIN,
    ACT_END,

    ACT_CHAR,
    ACT_PRED,

    ACT_MEMO,
    ACT_SAVE,
    ACT_EPSCHK,
    ACT_EPSSET
} ActionType;

typedef ActionType PredicateType;

typedef struct action        Action;
typedef Action               Predicate;
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
 * @param[in] regex the underlying regular expression
 *
 * @return the blank state machine
 */
StateMachine *smir_default(const char *regex);

/**
 * Create a new state machine with a given number of states.
 *
 * Each state is assigned a unique state identifier in [1...nstates].
 *
 * @param[in] regex the underlying regular expression
 * @param[in] nstates the number of states
 *
 * @return the new state machine
 */
StateMachine *smir_new(const char *regex, uint32_t nstates);

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
 * @param[in] self the state machine
 *
 * @return the number of states
 */
size_t smir_get_num_states(StateMachine *self);

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
 * Get the number of actions on a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the number of actions
 */
size_t smir_state_get_num_actions(StateMachine *self, state_id sid);

/**
 * Get the actions of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the actions of the state
 */
const ActionList *smir_state_get_actions(StateMachine *self, state_id sid);

/**
 * Append an action to a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] act  the action to append
 */
void smir_state_append_action(StateMachine *self,
                              state_id      sid,
                              const Action *act);

/**
 * Prepend an action to a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] act  the action to prepend
 */
void smir_state_prepend_action(StateMachine *self,
                               state_id      sid,
                               const Action *act);

/**
 * Set the actions of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] acts the list of actions
 */
void smir_state_set_actions(StateMachine *self, state_id sid, ActionList *acts);

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
 * Create an action for zero-width assertions.
 *
 * Valid types are ACT_BEGIN and ACT_END.
 *
 * @param[in] type the type of zero-width assertion
 *
 * @return the action
 */
const Action *smir_action_zwa(ActionType type);

/**
 * Create an action for matching against a character.
 *
 * @param[in] ch the character
 *
 * @return the action
 */
const Action *smir_action_char(const char *ch);

/**
 * Create an action for matching against a predicate.
 *
 * @param[in] pred     the predicate
 * @param[in] pred_len the length of the predicate
 *
 * @return the action
 */
const Action *smir_action_predicate(Interval *pred, len_t pred_len);

/**
 * Create an action which require relative pointers into memory.
 *
 * Valid types are ACT_SAVE, ACT_EPSCHK, ACT_EPSSET, ACT_MEMO.
 *
 * @param[in] type the type of the action
 * @param[in] k    the index into memory
 *
 * @return the action
 */
const Action *smir_action_num(ActionType type, len_t k);

/**
 * Get the number of actions on a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the number of actions
 */
size_t smir_trans_get_num_actions(StateMachine *self, trans_id tid);

/**
 * Get the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the actions of the transition
 */
const ActionList *smir_trans_get_actions(StateMachine *self, trans_id tid);

/**
 * Append an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to append
 */
void smir_trans_append_action(StateMachine *self,
                              trans_id      tid,
                              const Action *act);

/**
 * Prepend an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to prepend
 */
void smir_trans_prepend_action(StateMachine *self,
                               trans_id      tid,
                               const Action *act);

/**
 * Set the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] acts the list of actions
 */
void smir_trans_set_actions(StateMachine *self, trans_id tid, ActionList *acts);

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
