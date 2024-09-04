#include <stdio.h>
#include <stdlib.h>

#include "../../utils.h"
#include "flatten.h"

/* --- Preprocessor macros -------------------------------------------------- */

#define ACTION_TO_IDX(type)    ((unsigned int) (type))
#define ACTION_TO_BIT(type)    (1 << ACTION_TO_IDX(type))
#define INSERT_TYPE(set, type) (set |= ACTION_TO_BIT(type))

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    BruStateMachine *origin_sm;   /**< the original machine                   */
    BruStateMachine *new_sm;      /**< the new machine                        */
    bru_byte_t      *created;     /**< map from original to states to record of
                                 creation in new machine                */
    bru_state_id    *state_map;   /**< map from original states to new states */
    bru_state_id    *state_queue; /**< queue of states in original machine that
                                       have been added to new machine         */
    size_t eliminated_path_count; /**< the number of transitions eliminated
                                       since they were not useful             */
} BruFlattenGlobals;

/* --- Helper functions ----------------------------------------------------- */

/**
 * Create a signature for a list of actions for efficient comparisons.
 *
 * NOTE: The signature essentially encodes the ZWAs in the action list. It does
 * not account for any other actions. Currently, this means it only encodes
 * ACT_BEGIN and ACT_END.
 *
 * @param[in] actions the list of actions
 *
 * @return the signature
 */
static int action_list_signature(const BruActionList *actions)
{
    BruActionListIterator *ali;
    const BruAction       *a;
    int                    signature = 0;

    if (actions) {
        ali = bru_smir_action_list_iter(actions);
        while ((a = bru_smir_action_list_iterator_next(ali))) {
            switch (bru_smir_action_type(a)) {
                case BRU_ACT_CHAR:   /* fallthrough */
                case BRU_ACT_PRED:   /* fallthrough */
                case BRU_ACT_MEMO:   /* fallthrough */
                case BRU_ACT_EPSCHK: /* fallthrough */
                case BRU_ACT_EPSSET: /* fallthrough */
                case BRU_ACT_SAVE: break;

                case BRU_ACT_BEGIN: /* fallthrough */
                case BRU_ACT_END:
                    INSERT_TYPE(signature, bru_smir_action_type(a));
                    break;
            }
        }
        free(ali);
    }

    return signature;
}

/**
 * Compare two action lists for predicate equivalence.
 *
 * Predicate equivalence means evaluating the list of actions for the same input
 * will always yield the same result (True or False).
 *
 * TODO: this should really be defined somewhere else.
 *
 * @param[in] al1 the first list of actions
 * @param[in] al2 the second list of actions
 *
 * @return TRUE if they are equal, else FALSE
 */
static int action_list_signature_equivalent(int sig1, int sig2)
{
    // NOTE: any actions that, if appear in one list and not the other, means
    // the lists are not equal
    static int DISAMBIGUATING_ACTIONS =
        ACTION_TO_BIT(BRU_ACT_END) | ACTION_TO_BIT(BRU_ACT_BEGIN);

    return ((sig1 ^ sig2) & DISAMBIGUATING_ACTIONS) == 0;
}

static int is_epsilon_state(const BruActionList *actions)
{
    BruActionListIterator *ali = bru_smir_action_list_iter(actions);
    const BruAction       *act;
    bru_byte_t             is_epsilon = TRUE;

    while ((act = bru_smir_action_list_iterator_next(ali))) {
        switch (bru_smir_action_type(act)) {
            case BRU_ACT_CHAR: /* fallthrough */
            case BRU_ACT_PRED: is_epsilon = FALSE; goto done;

            case BRU_ACT_BEGIN:  /* fallthrough */
            case BRU_ACT_END:    /* fallthrough */
            case BRU_ACT_MEMO:   /* fallthrough */
            case BRU_ACT_SAVE:   /* fallthrough */
            case BRU_ACT_EPSCHK: /* fallthrough */
            case BRU_ACT_EPSSET: break;
        }
    }

done:
    free(ali);

    return is_epsilon;
}

static void remove_unnecessary_actions(const BruActionList *actions)
{
    BruActionListIterator *ali = bru_smir_action_list_iter(actions);
    const BruAction       *act;

    while ((act = bru_smir_action_list_iterator_next(ali))) {
        switch (bru_smir_action_type(act)) {
            // remove EPSSET/EPSCHK actions
            case BRU_ACT_EPSCHK: /* fallthrough */
            case BRU_ACT_EPSSET:
                bru_smir_action_list_iterator_remove(ali);
                break;

            case BRU_ACT_BEGIN: /* fallthrough */
            case BRU_ACT_END:   /* fallthrough */
            case BRU_ACT_CHAR:  /* fallthrough */
            case BRU_ACT_PRED:  /* fallthrough */
            case BRU_ACT_MEMO:  /* fallthrough */
            case BRU_ACT_SAVE: break;
        }
    }
    free(ali);
}

/**
 * Check if the given sequence of actions is satisfiable with respect to EPSSET
 * and EPSCHK actions.
 *
 * @param[in] actions the sequence of actions
 *
 * @return FALSE if the sequence of actions contains an EPSSET followed at some
 *         point by the corresponding EPSCHK, otherwise TRUE
 */
static int action_list_eps_satisfiable(const BruActionList *actions)
{
    BruActionListIterator *ali;
    const BruAction       *act;
    size_t                *epssets, satisfiable = TRUE;
    size_t                 idx, num;

    // TODO: use Set instead of Vec
    stc_vec_default_init(epssets);
    ali = bru_smir_action_list_iter(actions);

    while ((act = bru_smir_action_list_iterator_next(ali))) {
        switch (bru_smir_action_type(act)) {
            case BRU_ACT_BEGIN: /* fallthrough */
            case BRU_ACT_END:   /* fallthrough */
            case BRU_ACT_CHAR:  /* fallthrough */
            case BRU_ACT_PRED:  /* fallthrough */
            case BRU_ACT_MEMO:  /* fallthrough */
            case BRU_ACT_SAVE: break;

            case BRU_ACT_EPSCHK:
                num = bru_smir_action_get_num(act);
                for (idx = 0; idx < stc_vec_len(epssets); idx++) {
                    if (epssets[idx] == num) {
                        satisfiable = FALSE;
                        goto done;
                    }
                }
                break;

            case BRU_ACT_EPSSET:
                stc_vec_push_back(epssets, bru_smir_action_get_num(act));
                break;
        }
    }

done:
    free(ali);
    stc_vec_free(epssets);

    return satisfiable;
}

/**
 * Check if concatenating the given lists results in a sequence of actions
 * where an EPSSET is executed before the corresponding EPSCHK.
 *
 * @param[in] prefix the prefix list in the concatenation
 * @param[in] suffix the suffix list in the concatenation
 *
 * @return whether the resulting sequence of actions has an EPSSET before the
 *         corresponding EPSCHK.
 */
static int can_explore(const BruActionList *prefix, const BruActionList *suffix)
{
    BruActionList *concat, *tmp;
    int            satisfiable;

    concat = bru_smir_action_list_clone(prefix);
    tmp    = bru_smir_action_list_clone(suffix);
    bru_smir_action_list_append(concat, tmp);

    satisfiable = action_list_eps_satisfiable(concat);

    bru_smir_action_list_free(tmp);
    bru_smir_action_list_free(concat);

    return satisfiable;
}

/**
 * Check if adding a transition between the source and destination states with
 * the given actions is useful.
 *
 * In this context, `useful` means the proposed transition can be successfully
 * taken in instanaces where existing transitions cannot.
 *
 * @param[in] actions the sequence of actions for the new transition
 * @param[in] src     the source state identifier
 * @param[in] dst     the destination state identifier
 * @param[in] sm      the state machine
 *
 * @return TRUE if the transition is useful, otherwise FALSE
 */
static int transition_is_useful(const BruActionList *actions,
                                bru_state_id         src,
                                bru_state_id         dst,
                                BruStateMachine     *sm)
{
    bru_trans_id *out_trans;
    size_t        nout, i;
    bru_byte_t    useful        = TRUE;
    int           new_trans_sig = action_list_signature(actions), old_trans_sig;

    out_trans = bru_smir_get_out_transitions(sm, src, &nout);
    for (i = 0; i < nout; i++) {
        if (bru_smir_get_dst(sm, out_trans[i]) == dst) {
            old_trans_sig = action_list_signature(
                bru_smir_trans_get_actions(sm, out_trans[i]));
            if (old_trans_sig == 0 || action_list_signature_equivalent(
                                          old_trans_sig, new_trans_sig)) {
                useful = FALSE;
                break;
            }
        }
    }
    free(out_trans);

    return useful;
}

/**
 * For every outgoing transition from this state, if it can be explored (does
 * not contain EPSCHK action where corresponding EPSSET action is already on the
 * action path):
 *
 * 1. If the transition contains an EPSCHK and the corresponding EPSSET is on
 *      the action path, go to 7.
 * 2. Add the actions on the transition to the action path.
 * 3. If the destination state contains a character-consuming action,
 *     3.1. If this state has not been visited, copy the state to the new
 *           machine, and record the corresponding new state identifier.
 *           Mark as visited.
 *     3.2. Add a transition from the corresponding source state in the
 *            new machine to the corresponding destination state, with a
 *            copy of the current action path. Go to 6.
 * 4. Otherwise, add the state actions to the action path, and recurse on
 *      the destination state.
 * 5. Remove the actions from the state from the action
 *      path.
 * 6. Remove the actions from the transition from the action path.
 * 7. Continue iteration.
 *
 * @param[in] original_src the source state of the current DFS exploration
 * @param[in] current      the current source state of the DFS iteration
 * @param[in] path_actions the collection of actions along DFS path
 * @param[in] globals      the auxillary information for the transformation
 */
static void flatten_dfs(bru_state_id       original_src,
                        bru_state_id       current,
                        BruActionList     *path_actions,
                        BruFlattenGlobals *globals)
{
    BruActionListIterator *ali;
    BruActionList         *action_list_clone;
    const BruActionList   *trans_actions, *original_dst_actions;
    bru_trans_id          *out_trans;
    size_t                 nout, idx, count;
    bru_state_id           original_dst, new_src, new_dst;
    bru_trans_id           new_trans;

    out_trans =
        bru_smir_get_out_transitions(globals->origin_sm, current, &nout);
    for (idx = 0; idx < nout; idx++) {
        trans_actions =
            bru_smir_trans_get_actions(globals->origin_sm, out_trans[idx]);
        if (!can_explore(path_actions, trans_actions)) continue;

        // add transition actions to current path
        action_list_clone = bru_smir_action_list_clone(trans_actions);
        bru_smir_action_list_append(path_actions, action_list_clone);
        bru_smir_action_list_free(action_list_clone);

        original_dst = bru_smir_get_dst(globals->origin_sm, out_trans[idx]);
        original_dst_actions =
            bru_smir_state_get_actions(globals->origin_sm, original_dst);
        if (original_dst != BRU_FINAL_STATE_ID &&
            is_epsilon_state(original_dst_actions)) {
            // add state actions to path
            action_list_clone =
                bru_smir_action_list_clone(original_dst_actions);
            bru_smir_action_list_append(path_actions, action_list_clone);
            bru_smir_action_list_free(action_list_clone);

            // recurse
            flatten_dfs(original_src,
                        bru_smir_get_dst(globals->origin_sm, out_trans[idx]),
                        path_actions, globals);

            // remove state actions from path
            for (ali  = bru_smir_action_list_iter(path_actions),
                count = bru_smir_action_list_len(original_dst_actions);
                 bru_smir_action_list_iterator_prev(ali) && count--;)
                bru_smir_action_list_iterator_remove(ali);
            free(ali);
        } else {
            // insert state in new machine if not created before
            // TODO: possibly remove need for 'created' by having special value
            // in 'state_map' indicating if it has been created or not

            new_src = globals->state_map[original_src];

            if (!globals->created[original_dst]) {
                globals->created[original_dst] = TRUE;
                new_dst = globals->state_map[original_dst] =
                    bru_smir_add_state(globals->new_sm);
                bru_smir_state_set_actions(
                    globals->new_sm, new_dst,
                    bru_smir_action_list_clone(original_dst_actions));
                stc_vec_push_back(globals->state_queue, original_dst);
            } else {
                new_dst = globals->state_map[original_dst];
            }

            // create actions for transition in new machine
            action_list_clone = bru_smir_action_list_clone(path_actions);
            remove_unnecessary_actions(action_list_clone);

            // insert transition from source to new state, if it is
            // useful
            // TODO: consider making this step a separate transform, where we in
            // general eliminate unnecessary transitions from a state machine.
            // This could include the functionality of the `can_explore`
            // function.
            //
            // TODO: Check if transition is useful before cloning the actions?
            if (transition_is_useful(action_list_clone, new_src, new_dst,
                                     globals->new_sm)) {
                new_trans = bru_smir_add_transition(globals->new_sm, new_src);
                bru_smir_set_dst(globals->new_sm, new_trans, new_dst);
                bru_smir_trans_set_actions(globals->new_sm, new_trans,
                                           action_list_clone);
            } else {
                globals->eliminated_path_count++;
                bru_smir_action_list_free(action_list_clone);
            }
        }

        // remove transition actions
        for (ali  = bru_smir_action_list_iter(path_actions),
            count = bru_smir_action_list_len(trans_actions);
             bru_smir_action_list_iterator_prev(ali) && count--;) {
            bru_smir_action_list_iterator_remove(ali);
        }
        free(ali);
    }

    free(out_trans);
}

static void
flatten(BruStateMachine *original, BruStateMachine *new, FILE *logfile)
{
    BruActionList     *path_actions;
    BruFlattenGlobals *globals;
    size_t             nstates;
    bru_state_id       src;

    if (!original || !new) return;

    path_actions = bru_smir_action_list_new();

    // NOTE: +1 to account for implicit INITIAL_STATE and FINAL_STATE (state 0)
    // that occurs in every SMIR
    nstates            = bru_smir_get_num_states(original) + 1;
    globals            = malloc(sizeof(*globals));
    globals->origin_sm = original;
    globals->new_sm    = new;
    globals->created   = calloc(nstates, sizeof(*(globals->created)));
    globals->state_map = calloc(nstates, sizeof(*(globals->state_map)));
    stc_vec_default_init(globals->state_queue);
    globals->eliminated_path_count = 0;

    globals->created[BRU_INITIAL_STATE_ID]   = TRUE;
    globals->state_map[BRU_INITIAL_STATE_ID] = BRU_INITIAL_STATE_ID;
    globals->created[BRU_FINAL_STATE_ID]     = TRUE;
    globals->state_map[BRU_FINAL_STATE_ID]   = BRU_FINAL_STATE_ID;
    stc_vec_push_front(globals->state_queue, BRU_INITIAL_STATE_ID);

    while (!stc_vec_is_empty(globals->state_queue)) {
        src = globals->state_queue[0];
        stc_vec_remove(globals->state_queue, 0);
        flatten_dfs(src, src, path_actions, globals);
    }

#ifdef BRU_BENCHMARK
    fprintf(logfile, "NUMBER OF TRANSITIONS ELIMINATED FROM FLATTENING: %zu\n",
            globals->eliminated_path_count);
#else
    BRU_UNUSED(logfile);
#endif /* BRU_BENCHMARK */

    bru_smir_action_list_free(path_actions);
    stc_vec_free(globals->state_queue);
    free(globals->state_map);
    free(globals->created);
    free(globals);
}

/* --- API function definitions --------------------------------------------- */

BruStateMachine *bru_transform_flatten(BruStateMachine *sm, FILE *logfile)
{
    BruStateMachine *out;

    if (!sm) return sm;

    out = bru_smir_default(bru_smir_get_regex(sm));
    flatten(sm, out, logfile);

    return out;
}
