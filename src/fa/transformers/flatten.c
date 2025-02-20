#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <bru/fa/transformers/flatten.h>
#include <bru/utils.h>

/* --- Preprocessor directives ---------------------------------------------- */

#define ACTION_TO_IDX(type)    ((unsigned int) (type))
#define ACTION_TO_BIT(type)    (1 << ACTION_TO_IDX(type))
#define INSERT_TYPE(set, type) (set |= ACTION_TO_BIT(type))

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    BruStateMachine     *origin_sm;   /**< the original machine               */
    BruStateMachine     *new_sm;      /**< the new machine                    */
    bru_byte_t          *created;     /**< map from original states to record
                                           of creation in new machine         */
    bru_state_id        *state_map;   /**< map from original to new states    */
    StcVec(bru_state_id) state_queue; /**< queue of states in original machine
                                           that were added to new machine     */
    size_t eliminated_path_count;     /**< the number of transitions eliminated
                                           since they were not useful         */
} BruFlattenGlobals;

/* --- Helper functions ----------------------------------------------------- */

static StcVec(const BruAction *)
_transition_sublist(const BruActionList *actions, ...)
{
    va_list                   args;
    BruActionType             t;
    unsigned int              action_set = 0;
    StcVec(const BruAction *) sublist;
    BruActionListIterator    *ali;
    const BruAction          *a;

    va_start(args, actions);
    while ((t = va_arg(args, BruActionType)) != BRU_ACT_NACTIONS)
        action_set |= (1 << t);
    va_end(args);

    stc_vec_default_init(&sublist);
    ali = bru_smir_action_list_iter(actions);
    while ((a = bru_smir_action_list_iterator_next(ali)))
        if (action_set & (1 << bru_smir_action_type(a)))
            stc_vec_push_back(&sublist, a);

    return sublist;
}

#define transition_sublist(actions, ...) \
    _transition_sublist((actions), ##__VA_ARGS__, BRU_ACT_NACTIONS)

static int transition_contains_type(const BruActionList *actions,
                                    BruActionType        type)
{
    const BruAction       *a;
    BruActionListIterator *ali      = bru_smir_action_list_iter(actions);
    int                    contains = FALSE;

    while ((a = bru_smir_action_list_iterator_next(ali))) {
        if (bru_smir_action_type(a) == type) {
            contains = TRUE;
            break;
        }
    }

    bru_smir_action_list_iterator_free(ali);
    return contains;
}

typedef enum {
    KEEP_EXISTING_ONLY = 0,
    KEEP_BOTH          = 1,
    KEEP_NEW_ONLY      = 2
} UsefulResult;

static UsefulResult
transition_useful_caret(const BruActionList *existing_actions,
                        const BruActionList *new_actions,
                        int                  sequential_transitions)
{
    BRU_UNUSED(sequential_transitions);
    const BruActionType caret = BRU_ACT_BEGIN;
    UsefulResult useful = transition_contains_type(existing_actions, caret) &&
                                  !transition_contains_type(new_actions, caret)
                              ? KEEP_BOTH
                              : KEEP_EXISTING_ONLY;

    return useful;
}

static UsefulResult
transition_useful_dollar(const BruActionList *existing_actions,
                         const BruActionList *new_actions,
                         int                  sequential_transitions)

{
    BRU_UNUSED(sequential_transitions);
    const BruActionType dollar = BRU_ACT_END;
    UsefulResult useful = transition_contains_type(existing_actions, dollar) &&
                                  !transition_contains_type(new_actions, dollar)
                              ? KEEP_BOTH
                              : KEEP_EXISTING_ONLY;

    return useful;
}

static int _is_memochk_subset(StcVec(const BruAction *) s1,
                              StcVec(const BruAction *) s2)
{
    int    is_subset = TRUE;
    size_t i, len1, j, len2;

    // check if it is a subset by assuming it is, and then checking if any
    // element in s1 is NOT in s2
    for (i = 0, len1 = stc_vec_len(s1), len2 = stc_vec_len(s2);
         i < len1 && is_subset; i++) {
        if (bru_smir_action_type(s1[i]) == BRU_ACT_MEMOCHK) {
            for (j = 0; j < len2; j++)
                if (bru_smir_action_equal(s1[i], s2[j])) goto in_set;
            is_subset = FALSE;
        in_set:;
        }
    }

    return is_subset;
}

static UsefulResult
transition_useful_memo(const BruActionList *existing_actions,
                       const BruActionList *new_actions,
                       int                  is_sequential)
{
    StcVec(const BruAction *) existing_memo =
        transition_sublist(existing_actions, BRU_ACT_MEMOCHK, BRU_ACT_MEMOSET);
    StcVec(const BruAction *) new_memo =
        transition_sublist(new_actions, BRU_ACT_MEMOCHK, BRU_ACT_MEMOSET);
    const BruAction *existing_memoset = NULL, *new_memoset = NULL;
    size_t           i, existing_len, new_len;
    UsefulResult     useful = KEEP_EXISTING_ONLY;

    existing_len = stc_vec_len(existing_memo);
    new_len      = stc_vec_len(new_memo);

    for (i = 0; i < existing_len; i++) {
        if (bru_smir_action_type(existing_memo[i]) == BRU_ACT_MEMOSET) {
            existing_memoset = existing_memo[i];
            existing_len     = i + 1;
            break;
        }
    }

    for (i = 0; i < new_len; i++) {
        if (bru_smir_action_type(new_memo[i]) == BRU_ACT_MEMOSET) {
            new_memoset = new_memo[i];
            new_len     = i + 1;
            break;
        }
    }

    if (new_memoset == existing_memoset ||
        (new_memoset && existing_memoset &&
         bru_smir_action_equal(new_memoset, existing_memoset))) {
        // memosets are the same -- compare subsets of memochk
        if (!_is_memochk_subset(existing_memo, new_memo)) useful = KEEP_BOTH;

        if (useful == KEEP_BOTH && is_sequential) {
            // we can delete the existing transition if the new one's MEMOCHK is
            // a subset of the existing, since it will pass in more cases. This
            // is only valid if the transitions will be tried one after the
            // other.
            //
            // For example
            // existing:     a -- #3? -- #4? -- #2? --> b    ;
            // new:          a -- #3? --     --     --> b    ; keep this one
            if (_is_memochk_subset(new_memo, existing_memo))
                useful = KEEP_NEW_ONLY;
        }
    } else {
        // different memoset, so keep both
        useful = KEEP_BOTH;
    }

    stc_vec_free(existing_memo);
    stc_vec_free(new_memo);

    return useful;
}

static int is_epsilon_state(const BruActionList *actions)
{
    BruActionListIterator *ali = bru_smir_action_list_iter(actions);
    const BruAction       *act;
    bru_byte_t             is_epsilon = TRUE;

    while ((act = bru_smir_action_list_iterator_next(ali))) {
        switch (bru_smir_action_type(act)) {
            case BRU_ACT_MEMOSET: /* fallthrough */
            case BRU_ACT_CHAR:    /* fallthrough */
            case BRU_ACT_BACKREF: /* fallthrough */
            case BRU_ACT_PRED: is_epsilon = FALSE; goto done;

            case BRU_ACT_BEGIN:   /* fallthrough */
            case BRU_ACT_END:     /* fallthrough */
            case BRU_ACT_SAVE:    /* fallthrough */
            case BRU_ACT_INC:     /* fallthrough */
            case BRU_ACT_SET:     /* fallthrough */
            case BRU_ACT_CMP:     /* fallthrough */
            case BRU_ACT_EPSSET:  /* fallthrough */
            case BRU_ACT_EPSCHK:  /* fallthrough */
            case BRU_ACT_MEMOCHK: /* fallthrough */
            case BRU_ACT_WRITE: break;

            case BRU_ACT_NACTIONS: assert(FALSE && "unreachable"); break;
        }
    }

done:
    free(ali);

    return is_epsilon;
}

static void remove_unnecessary_actions(const BruActionList *actions)
{
    BruActionListIterator    *ali = bru_smir_action_list_iter(actions);
    const BruAction          *act;
    StcVec(const BruAction *) unique_elements;
    size_t                    i;
    int                       remove_all = FALSE;

    stc_vec_default_init(&unique_elements);
    while ((act = bru_smir_action_list_iterator_next(ali))) {
        if (remove_all) {
            bru_smir_action_list_iterator_remove(ali);
            continue;
        }
        switch (bru_smir_action_type(act)) {
            case BRU_ACT_CHAR:    /* fallthrough */
            case BRU_ACT_PRED:    /* fallthrough */
            case BRU_ACT_SAVE:    /* fallthrough */
            case BRU_ACT_BACKREF: /* fallthrough */
            case BRU_ACT_INC:     /* fallthrough */
            case BRU_ACT_SET:     /* fallthrough */
            case BRU_ACT_CMP:     /* fallthrough */
            case BRU_ACT_WRITE: break;

            // remove EPSSET/EPSCHK actions
            case BRU_ACT_EPSSET: /* fallthrough */
            case BRU_ACT_EPSCHK:
                bru_smir_action_list_iterator_remove(ali);
                break;

            case BRU_ACT_MEMOSET: remove_all = TRUE; break;

            case BRU_ACT_BEGIN: /* fallthrough */
            case BRU_ACT_END:   /* fallthrough */
            case BRU_ACT_MEMOCHK:
                for (i = 0; i < stc_vec_len(unique_elements); i++)
                    if (bru_smir_action_equal(act, unique_elements[i])) {
                        bru_smir_action_list_iterator_remove(ali);
                        goto no_push;
                    }
                stc_vec_push_back(&unique_elements, act);
            no_push:
                break;

            case BRU_ACT_NACTIONS: assert(FALSE && "unreachable"); break;
        }
    }
    free(ali);
    stc_vec_free(unique_elements);
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
    StcVec(size_t)         epssets;
    size_t                 idx, num, satisfiable = TRUE;

    // TODO: use Set instead of Vec
    stc_vec_default_init(&epssets);
    ali = bru_smir_action_list_iter(actions);

    while ((act = bru_smir_action_list_iterator_next(ali))) {
        switch (bru_smir_action_type(act)) {
            case BRU_ACT_BEGIN:   /* fallthrough */
            case BRU_ACT_END:     /* fallthrough */
            case BRU_ACT_CHAR:    /* fallthrough */
            case BRU_ACT_PRED:    /* fallthrough */
            case BRU_ACT_SAVE:    /* fallthrough */
            case BRU_ACT_BACKREF: /* fallthrough */
            case BRU_ACT_INC:     /* fallthrough */
            case BRU_ACT_SET:     /* fallthrough */
            case BRU_ACT_CMP:     /* fallthrough */
            case BRU_ACT_MEMOSET: /* fallthrough */
            case BRU_ACT_MEMOCHK: /* fallthrough */
            case BRU_ACT_WRITE: break;

            case BRU_ACT_EPSSET:
                stc_vec_push_back(&epssets, bru_smir_action_get_num(act));
                break;

            case BRU_ACT_EPSCHK:
                num = bru_smir_action_get_num(act);
                for (idx = 0; idx < stc_vec_len(epssets); idx++) {
                    if (epssets[idx] == num) {
                        satisfiable = FALSE;
                        goto done;
                    }
                }
                break;

            case BRU_ACT_NACTIONS: assert(FALSE && "unreachable"); break;
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
 * @return a UsefulResult value
 */
static UsefulResult transition_is_useful(const BruActionList *new_actions,
                                         bru_state_id         src,
                                         bru_state_id         dst,
                                         BruStateMachine     *sm)
{
    bru_trans_id        *out_trans;
    StcVec(bru_trans_id) deleted_transitions;
    size_t               nout, i, last_idx;
    int                  useful = KEEP_BOTH;
    const BruActionList *existing_actions;

    stc_vec_default_init(&deleted_transitions);
    out_trans = bru_smir_get_out_transitions(sm, src, &nout);
    last_idx  = nout - 1;

    for (i = nout - 1; i < nout; i--) {
        if (bru_smir_get_dst(sm, out_trans[i]) == dst) {
            existing_actions = bru_smir_trans_get_actions(sm, out_trans[i]);
            switch (
                (useful = transition_useful_dollar(existing_actions,
                                                   new_actions, i == last_idx) |
                          transition_useful_caret(existing_actions, new_actions,
                                                  i == last_idx) |
                          transition_useful_memo(existing_actions, new_actions,
                                                 i == last_idx))) {
                case KEEP_EXISTING_ONLY:
                case KEEP_BOTH: break;
                case KEEP_NEW_ONLY:
                    // TODO: delete transition and carry on
                    last_idx--;
                    stc_vec_push_back(&deleted_transitions, out_trans[i]);
                    assert(FALSE && "TODO");

                case KEEP_BOTH | KEEP_NEW_ONLY: useful = KEEP_BOTH; break;

                default: assert(FALSE && "unreachable");
            }
            break;
        }
    }

    while (!stc_vec_is_empty(deleted_transitions))
        bru_smir_remove_transition(sm, stc_vec_pop_back(&deleted_transitions));

    stc_vec_free(deleted_transitions);
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
                stc_vec_push_back(&globals->state_queue, original_dst);
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
    stc_vec_default_init(&globals->state_queue);
    globals->eliminated_path_count = 0;

    globals->created[BRU_INITIAL_STATE_ID]   = TRUE;
    globals->state_map[BRU_INITIAL_STATE_ID] = BRU_INITIAL_STATE_ID;
    globals->created[BRU_FINAL_STATE_ID]     = TRUE;
    globals->state_map[BRU_FINAL_STATE_ID]   = BRU_FINAL_STATE_ID;
    stc_vec_push_back(&globals->state_queue, BRU_INITIAL_STATE_ID);

    while (!stc_vec_is_empty(globals->state_queue)) {
        src = stc_vec_pop_front(&globals->state_queue);
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
