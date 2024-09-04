#ifndef BRU_FA_SMIR_H
#define BRU_FA_SMIR_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../vm/srvm.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define BRU_NULL_STATE       0
#define BRU_INITIAL_STATE_ID BRU_NULL_STATE
#define BRU_FINAL_STATE_ID   BRU_NULL_STATE

#define BRU_IS_INITIAL_STATE(sid) ((sid) == BRU_INITIAL_STATE_ID)
#define BRU_IS_FINAL_STATE(sid)   ((sid) == BRU_FINAL_STATE_ID)

/* --- Type definitions ----------------------------------------------------- */

typedef enum {
    BRU_ACT_BEGIN,
    BRU_ACT_END,

    BRU_ACT_CHAR,
    BRU_ACT_PRED,

    BRU_ACT_MEMO,
    BRU_ACT_SAVE,
    BRU_ACT_EPSCHK,
    BRU_ACT_EPSSET,
} BruActionType;

typedef BruActionType BruPredicateType;

typedef struct bru_action               BruAction;
typedef BruAction                       BruPredicate;
typedef struct bru_action_list          BruActionList;
typedef struct bru_action_list_iterator BruActionListIterator;
typedef struct bru_state_machine        BruStateMachine;

typedef uint32_t bru_state_id; // 0 => nonexistent
typedef uint64_t bru_trans_id; // (src state_id, idx into outgoing transitions)

typedef void bru_compile_f(void *meta, BruProgram *prog);

#if !defined(BRU_FA_SMIR_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_SMIR_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&     \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||  \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef BruActionType ActionType;
#    define ACT_BEGIN  BRU_ACT_BEGIN
#    define ACT_END    BRU_ACT_END
#    define ACT_CHAR   BRU_ACT_CHAR
#    define ACT_PRED   BRU_ACT_PRED
#    define ACT_MEMO   BRU_ACT_MEMO
#    define ACT_SAVE   BRU_ACT_SAVE
#    define ACT_EPSCHK BRU_ACT_EPSCHK
#    define ACT_EPSSET BRU_ACT_EPSSET

typedef BruPredicateType      PredicateType;
typedef BruAction             Action;
typedef BruPredicate          Predicate;
typedef BruActionList         ActionList;
typedef BruActionListIterator ActionListIterator;
typedef BruStateMachine       StateMachine;

typedef bru_state_id  state_id;
typedef bru_trans_id  trans_id;
typedef bru_compile_f compile_f;

#    define smir_default bru_smir_default
#    define smir_new     bru_smir_new
#    define smir_free    bru_smir_free
#    define smir_compile bru_smir_compile

#    define smir_add_state             bru_smir_add_state
#    define smir_get_num_states        bru_smir_get_num_states
#    define smir_get_regex             bru_smir_get_regex
#    define smir_set_initial           bru_smir_set_initial
#    define smir_get_initial           bru_smir_get_initial
#    define smir_set_final             bru_smir_set_final
#    define smir_add_transition        bru_smir_add_transition
#    define smir_get_out_transitions   bru_smir_get_out_transitions
#    define smir_state_get_num_actions bru_smir_state_get_num_actions
#    define smir_state_get_actions     bru_smir_state_get_actions
#    define smir_state_append_action   bru_smir_state_append_action
#    define smir_state_prepend_action  bru_smir_state_prepend_action
#    define smir_state_set_actions     bru_smir_state_set_actions
#    define smir_state_clone_actions   bru_smir_state_clone_actions

#    define smir_get_src               bru_smir_get_src
#    define smir_get_dst               bru_smir_get_dst
#    define smir_set_dst               bru_smir_set_dst
#    define smir_trans_get_num_actions bru_smir_trans_get_num_actions
#    define smir_trans_get_actions     bru_smir_trans_get_actions
#    define smir_trans_append_action   bru_smir_trans_append_action
#    define smir_trans_prepend_action  bru_smir_trans_prepend_action
#    define smir_trans_set_actions     bru_smir_trans_set_actions
#    define smir_trans_clone_actions   bru_smir_trans_clone_actions

#    define smir_print bru_smir_print

#    define smir_action_zwa       bru_smir_action_zwa
#    define smir_action_char      bru_smir_action_char
#    define smir_action_predicate bru_smir_action_predicate
#    define smir_action_num       bru_smir_action_num
#    define smir_action_clone     bru_smir_action_clone
#    define smir_action_free      bru_smir_action_free
#    define smir_action_type      bru_smir_action_type
#    define smir_action_get_num   bru_smir_action_get_num
#    define smir_action_print     bru_smir_action_print

#    define smir_action_list_new        bru_smir_action_list_new
#    define smir_action_list_clone      bru_smir_action_list_clone
#    define smir_action_list_clone_into bru_smir_action_list_clone_into
#    define smir_action_list_clear      bru_smir_action_list_clear
#    define smir_action_list_free       bru_smir_action_list_free
#    define smir_action_list_len        bru_smir_action_list_len
#    define smir_action_list_push_back  bru_smir_action_list_push_back
#    define smir_action_list_push_front bru_smir_action_list_push_front
#    define smir_action_list_append     bru_smir_action_list_append
#    define smir_action_list_prepend    bru_smir_action_list_prepend

#    define smir_action_list_iter          bru_smir_action_list_iter
#    define smir_action_list_iterator_next bru_smir_action_list_iterator_next
#    define smir_action_list_iterator_prev bru_smir_action_list_iterator_prev
#    define smir_action_list_iterator_remove \
        bru_smir_action_list_iterator_remove
#    define smir_action_list_print bru_smir_action_list_print

#    define smir_set_pre_meta      bru_smir_set_pre_meta
#    define smir_get_pre_meta      bru_smir_get_pre_meta
#    define smir_set_post_meta     bru_smir_set_post_meta
#    define smir_get_post_meta     bru_smir_get_post_meta
#    define smir_compile_with_meta bru_smir_compile_with_meta

#    define smir_reorder_states bru_smir_reorder_states
#endif /* BRU_FA_SMIR_ENABLE_SHORT_NAMES */

/* --- API function prototypes ---------------------------------------------- */

/**
 * Create a blank state machine.
 *
 * @param[in] regex the underlying regular expression
 *
 * @return the blank state machine
 */
BruStateMachine *bru_smir_default(const char *regex);

/**
 * Create a new state machine with a given number of states.
 *
 * Each state is assigned a unique state identifier in [1...nstates].
 *
 * @param[in] regex   the underlying regular expression
 * @param[in] nstates the number of states
 *
 * @return the new state machine
 */
BruStateMachine *bru_smir_new(const char *regex, uint32_t nstates);

/**
 * Free all memory used by the state machine.
 *
 * @param[in] self the state machine
 */
void bru_smir_free(BruStateMachine *self);

/**
 * Compile a state machine.
 *
 * @param[in] self the state machine
 *
 * @return the compiled program
 */
BruProgram *bru_smir_compile(BruStateMachine *self);

/**
 * Create a new state in the state machine.
 *
 * @param[in] self the state machine
 *
 * @return the unique state identifier
 */
bru_state_id bru_smir_add_state(BruStateMachine *self);

/**
 * Get the current number of states in the state machine.
 *
 * @param[in] self the state machine
 *
 * @return the number of states
 */
size_t bru_smir_get_num_states(BruStateMachine *self);

/**
 * Get the underlying reguler expression of a state machine.
 *
 * @param[in] self the state machine
 *
 * @return the underlying regular expression
 */
const char *bru_smir_get_regex(BruStateMachine *self);

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
bru_trans_id bru_smir_set_initial(BruStateMachine *self, bru_state_id sid);

/**
 * Get the initial transitions of the state machine.
 *
 * Convenience function equivalent to calling @ref smir_get_out_transitions
 * with @ref BRU_INITIAL_STATE_ID.
 *
 * @param[in]  self the state machine
 * @param[out] n    the number of initial transitions
 *
 * @return the identifiers representing the initial transitions
 */
bru_trans_id *bru_smir_get_initial(BruStateMachine *self, size_t *n);

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
bru_trans_id bru_smir_set_final(BruStateMachine *self, bru_state_id sid);

/**
 * Add an outgoing transition to a state.
 *
 * The added transition will not have a destination state.
 * @see smir_set_dst.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the unique transition identifer
 */
bru_trans_id bru_smir_add_transition(BruStateMachine *self, bru_state_id sid);

/**
 * Get the outgoing transitions of a state.
 *
 * The returned array is created with a call to `malloc`, and the memory
 * should be released using a single call to `free`.
 *
 * @param[in]  self the state machine
 * @param[in]  sid  the unique state identifier
 * @param[out] n    the length of the returned array
 *
 * @return an array of transition identifiers representing the outgoing
 *         transitions
 */
bru_trans_id *bru_smir_get_out_transitions(BruStateMachine *self,
                                           bru_state_id     sid,
                                           size_t          *n);

/**
 * Get the number of actions on a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the number of actions
 */
size_t bru_smir_state_get_num_actions(BruStateMachine *self, bru_state_id sid);

/**
 * Get the actions of a state. Will return NULL if the `sid` is
 * BRU_INITIAL_STATE_ID or BRU_FINAL_STATE_ID.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the actions of the state, or NULL
 */
const BruActionList *bru_smir_state_get_actions(BruStateMachine *self,
                                                bru_state_id     sid);

/**
 * Append an action to a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] act  the action to append
 */
void bru_smir_state_append_action(BruStateMachine *self,
                                  bru_state_id     sid,
                                  const BruAction *act);

/**
 * Prepend an action to a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] act  the action to prepend
 */
void bru_smir_state_prepend_action(BruStateMachine *self,
                                   bru_state_id     sid,
                                   const BruAction *act);

/**
 * Set the actions of a state.
 *
 * Frees any memory in use by the existing list of actions.
 * Nothing changes if the provided list is NULL.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] acts the list of actions
 */
void bru_smir_state_set_actions(BruStateMachine *self,
                                bru_state_id     sid,
                                BruActionList   *acts);

/**
 * Clone the list of actions of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the state identifier of the state whose actions should be
 *                 cloned
 *
 * @return the cloned list of actions
 */
BruActionList *bru_smir_state_clone_actions(BruStateMachine *self,
                                            bru_state_id     sid);

/**
 * Get the source state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique state identifier of the source state
 */
bru_state_id bru_smir_get_src(BruStateMachine *self, bru_trans_id tid);

/**
 * Get the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the unique state identifier of the destination state
 */
bru_state_id bru_smir_get_dst(BruStateMachine *self, bru_trans_id tid);

/**
 * Set the destination state of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] dst  the unique state identifier of the destination state
 */
void bru_smir_set_dst(BruStateMachine *self,
                      bru_trans_id     tid,
                      bru_state_id     dst);

/**
 * Get the number of actions on a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the number of actions
 */
size_t bru_smir_trans_get_num_actions(BruStateMachine *self, bru_trans_id tid);

/**
 * Get the actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 *
 * @return the actions of the transition
 */
const BruActionList *bru_smir_trans_get_actions(BruStateMachine *self,
                                                bru_trans_id     tid);

/**
 * Append an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to append
 */
void bru_smir_trans_append_action(BruStateMachine *self,
                                  bru_trans_id     tid,
                                  const BruAction *act);

/**
 * Prepend an action to a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] act  the action to prepend
 */
void bru_smir_trans_prepend_action(BruStateMachine *self,
                                   bru_trans_id     tid,
                                   const BruAction *act);

/**
 * Set the actions of a transition.
 *
 * Frees any memory in use by the existing list of actions.
 * Nothing changes if the provided list is NULL.
 *
 * @param[in] self the state machine
 * @param[in] tid  the unique transition identifier
 * @param[in] acts the list of actions
 */
void bru_smir_trans_set_actions(BruStateMachine *self,
                                bru_trans_id     tid,
                                BruActionList   *acts);

/**
 * Clone the list of actions of a transition.
 *
 * @param[in] self the state machine
 * @param[in] tid  the transition identifier of the transition whose actions
 *                 should be cloned
 *
 * @return the cloned list of actions
 */
BruActionList *bru_smir_trans_clone_actions(BruStateMachine *self,
                                            bru_trans_id     tid);

/**
 * Print the state machine.
 *
 * @param[in] self   the state machine
 * @param[in] stream the file stream to print to
 */
void bru_smir_print(BruStateMachine *self, FILE *stream);

/* --- Action and ActionList functions -------------------------------------- */

/**
 * Create an action for zero-width assertions.
 *
 * Valid types are ACT_BEGIN and ACT_END.
 *
 * @param[in] type the type of zero-width assertion
 *
 * @return the action
 */
const BruAction *bru_smir_action_zwa(BruActionType type);

/**
 * Create an action for matching against a character.
 *
 * @param[in] ch the character
 *
 * @return the action
 */
const BruAction *bru_smir_action_char(const char *ch);

/**
 * Create an action for matching against a predicate.
 *
 * @param[in] pred     the predicate
 *
 * @return the action
 */
const BruAction *bru_smir_action_predicate(const BruIntervals *pred);

/**
 * Create an action which require relative pointers into memory.
 *
 * Valid types are ACT_SAVE, ACT_EPSCHK, ACT_EPSSET, ACT_MEMO.
 *
 * If it is ACT_SAVE, the identifier is the index into capture memory.
 * Otherwise, it is the unique regex identifier.
 *
 * @param[in] type the type of the action
 * @param[in] k    the identifer for this action
 *
 * @return the action
 */
const BruAction *bru_smir_action_num(BruActionType type, size_t k);

/**
 * Clone an action.
 *
 * @param[in] self the action to clone
 *
 * @return the cloned action
 */
const BruAction *bru_smir_action_clone(const BruAction *self);

/**
 * Free the memory of the action.
 *
 * @param[in] self the action to free
 */
void bru_smir_action_free(const BruAction *self);

/**
 * Get the type of the action.
 *
 * @param[in] self the action
 *
 * @return the type of the action
 */
BruActionType bru_smir_action_type(const BruAction *self);

/**
 * Get the number associated with an action if possible.
 *
 * Note: this function returns 0 if the type of the action is not ACT_MEMO,
 * ACT_SAVE, ACT_EPSCHK, or ACT_EPSSET.
 *
 * @param[in] self the action
 *
 * @return the number associated with the action
 */
size_t bru_smir_action_get_num(const BruAction *self);

/**
 * Print the action.
 *
 * @param[in] self   the action
 * @param[in] stream the file stream to print to
 */
void bru_smir_action_print(const BruAction *self, FILE *stream);

/**
 * Create an empty list of actions.
 *
 * @return the empty list of actions
 */
BruActionList *bru_smir_action_list_new(void);

/**
 * Clone the list of actions.
 *
 * @param[in] self the list of actions to clone
 *
 * @return the cloned list of actions
 */
BruActionList *bru_smir_action_list_clone(const BruActionList *self);

/**
 * Clone the list of actions into an existing list.
 *
 * Note: If the list to clone into is not empty, this will insert the cloned
 * items at the end of the list.
 *
 * @param[in] self  the list of actions to clone
 * @param[in] clone the preallocated list to clone into
 */
void bru_smir_action_list_clone_into(const BruActionList *self,
                                     BruActionList       *clone);

/**
 * Remove (and free) the elements of a list of actions.
 *
 * @param[in] self the list of actions
 */
void bru_smir_action_list_clear(BruActionList *self);

/**
 * Free the list of actions.
 *
 * @param[in] self the list of actions
 */
void bru_smir_action_list_free(BruActionList *self);

/**
 * Get the number of actions in a list of actions.
 *
 * NOTE: Currently O(n) complexity.
 *
 * @param[in] self the list of actions
 *
 * @return the number of actions in the list (0 if the list is NULL or empty)
 */
size_t bru_smir_action_list_len(const BruActionList *self);

/**
 * Push an action to the back of a list of actions.
 *
 * @param[in] self the list of actions
 * @param[in] act  the action to push to the back
 */
void bru_smir_action_list_push_back(BruActionList *self, const BruAction *act);

/**
 * Push an action to the front of a list of actions.
 *
 * @param[in] self the list of actions
 * @param[in] act  the action to push to the front
 */
void bru_smir_action_list_push_front(BruActionList *self, const BruAction *act);

/**
 * Append a list of actions to another list of actions.
 *
 * Note: the appended list will be drained after the operation (becomes empty).
 *
 * @param[in] self the list of actions to append to
 * @param[in] acts the list of actions to append
 */
void bru_smir_action_list_append(BruActionList *self, BruActionList *acts);

/**
 * Prepend a list of actions to another list of actions.
 *
 * Note: the prepended list will be drained after the operation (becomes empty).
 *
 * @param[in] self the list of actions to prepend to
 * @param[in] acts the list of actions to prepend
 */
void bru_smir_action_list_prepend(BruActionList *self, BruActionList *acts);

/**
 * Create an interator for a list of actions to be able to go over the actions
 * of the list.
 *
 * Note: the iterator must be freed with a single call to free.
 *
 * @param[in] self the list of actions to iterator over
 *
 * @return the interator for the list of actions
 */
BruActionListIterator *bru_smir_action_list_iter(const BruActionList *self);

/**
 * Get the next action in the action list iterator.
 *
 * Note: you can call this function interchangeably with
 * @ref smir_action_list_iterator_prev, but note that if that function returns
 * NULL, then this function will return NULL for the rest of the iterator's
 * lifetime.
 *
 * @param[in] self the action list iterator
 *
 * @return the next action in the action list iterator if there is one;
 *         else NULL
 */
const BruAction *
bru_smir_action_list_iterator_next(BruActionListIterator *self);

/**
 * Get the previous action in the action list iterator.
 *
 * Note: you can call this function interchangeably with
 * @ref smir_action_list_iterator_next, but note that if that function returns
 * NULL, then this function will return NULL for the rest of the iterator's
 * lifetime.
 *
 * @param[in] self the action list iterator
 *
 * @return the previous action in the action list iterator if there is one;
 *         else NULL
 */
const BruAction *
bru_smir_action_list_iterator_prev(BruActionListIterator *self);

/**
 * Remove and free the memory of the current iteration action for the iterator
 * from the list of actions this iterator belongs to.
 *
 * The current iteration action is the last action returned from the next or
 * prev iterator functions. If NULL was returned last or neither of these two
 * functions have been called yet, then this function does nothing.
 *
 * @param[in] self the action list iterator
 */
void bru_smir_action_list_iterator_remove(BruActionListIterator *self);

/**
 * Print the list of actions.
 *
 * @param[in] self   the list of actions
 * @param[in] stream the file stream to print to
 */
void bru_smir_action_list_print(const BruActionList *self, FILE *stream);

/* --- Extendable API function prototypes ----------------------------------- */

/**
 * Set the pre-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] meta the pre-predicate meta data
 *
 * @return the previous pre-predicate meta data (initially NULL)
 */
void *
bru_smir_set_pre_meta(BruStateMachine *self, bru_state_id sid, void *meta);

/**
 * Get the pre-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the pre-predicate meta data
 */
void *bru_smir_get_pre_meta(BruStateMachine *self, bru_state_id sid);

/**
 * Set the post-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 * @param[in] meta the post-predicate meta data
 *
 * @return the previous post-predicate meta data (initially NULL)
 */
void *
bru_smir_set_post_meta(BruStateMachine *self, bru_state_id sid, void *meta);

/**
 * Get the post-predicate meta data of a state.
 *
 * @param[in] self the state machine
 * @param[in] sid  the unique state identifier
 *
 * @return the post-predicate meta data
 */
void *bru_smir_get_post_meta(BruStateMachine *self, bru_state_id sid);

/**
 * Compile the state machine to VM instructions, including the meta data.
 *
 * @param[in] self      the state machine
 * @param[in] pre_meta  the compiler for pre-predicate meta data at states
 * @param[in] post_meta the compiler for post-predicate meta data at states
 *
 * @return the compiled program
 */
BruProgram *bru_smir_compile_with_meta(BruStateMachine *self,
                                       bru_compile_f   *pre_meta,
                                       bru_compile_f   *post_meta);

/**
 * Reorder the states of the state machine with the given ordering.
 *
 * The order of compilation of the states in the VM will be determined by the
 * ordering parameter. Once this function is called, the state transitions
 * identifiers will be invalidated as they are changed to match the new
 * ordering. Passing in NULL will not change the state machine. This function
 * assumes that the ordering contains unique values from 1 to the number of
 * states in the state machine, and contains every state identifier.
 *
 * @param[in] self         the state machine
 * @param[in] sid_ordering the new order of the states
 */
void bru_smir_reorder_states(BruStateMachine *self, bru_state_id *sid_ordering);

#endif /* BRU_FA_SMIR_H */
