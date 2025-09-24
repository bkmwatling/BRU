#include <assert.h>
#include <stdlib.h>

#include <bru/vm/compilers/smir.h>

/* --- Preprocesser macros -------------------------------------------------- */

#define CURRENT_INSTRUCTION (stc_vec_len(*instructions))
#define LAST_INSTRUCTION    (CURRENT_INSTRUCTION - 1)
#define MAKE_PATCH(patch, f1, f2, f3)     \
    do {                                  \
        patch = malloc(sizeof(*(patch))); \
        patch->f1;                        \
        patch->f2;                        \
        patch->f3;                        \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_backpatch BruBackpatch;

struct bru_backpatch {
    BruBackpatch *next; /**< next backpath in the list                        */

    size_t patch_instr_idx; /**< the instruction to backpatch                 */

    // When compiling a transition, the destination state offset will only be
    // known after all compilation, so store the state id to lookup later.
    //
    // When compiling a state, the outgoing transition offsets are known
    // immediately, so store them explicitly in target_instr_idx.
    //
    // Before resolving the offsets, the state offsets are looked up and stored
    // in target_instr_idx.
    union {
        size_t state_id;         /**< state id to lookup target instruction   */
        size_t target_instr_idx; /**< the offset of the target instruction    */
    };
};

typedef struct {
    size_t    uid; /**< the universal identifier for the mapping              */
    bru_len_t idx; /**< the index the universal identifier is mapped to       */
} BruUidToIdx;

typedef struct {
    StcVec(BruUidToIdx) memo_map;         /**< memoisation mapping            */
    StcVec(BruUidToIdx) thread_cmap;      /**< thread counter memory mapping  */
    StcVec(BruUidToIdx) thread_mmap;      /**< thread general memory mapping  */
    bru_len_t           next_memo_idx;    /**< next index for memoisation map */
    bru_len_t           next_thread_cidx; /**< next index for thread cmap     */
    bru_len_t           next_thread_midx; /**< next index for thread mmap     */
} BruMemoryMaps; // map regex identifiers/indices to memory indices

/* --- Helper function definitions ------------------------------------------ */

static void compile_actions(StcVec(BruInstruction) *instructions,
                            const BruActionList    *acts,
                            // int                   *continue_compilation,
                            BruMemoryMaps          *mmaps)
{
#define GET_IDX(mmap_ptr, next_idx, idx_inc, id)                          \
    do {                                                                  \
        for (idx = 0, len = stc_vec_len(*(mmap_ptr));                     \
             idx < len && (*(mmap_ptr))[idx].uid != (id); idx++);         \
        if (idx == len) {                                                 \
            idx         = (next_idx);                                     \
            (next_idx) += (idx_inc);                                      \
            stc_vec_push_back(mmap_ptr, ((BruUidToIdx) { (id), (idx) })); \
        } else {                                                          \
            idx = (*(mmap_ptr))[idx].idx;                                 \
        }                                                                 \
    } while (0)

    BruActionListIter *iter;
    const BruAction   *act;
    size_t             idx, len;

    if (!acts) return;

    for (iter     = bru_smir_action_list_iter(acts),
        act       = bru_smir_action_list_iter_next(iter);
         act; act = bru_smir_action_list_iter_next(iter)) {
        switch (bru_smir_action_type(act)) {
            case BRU_ACT_BEGIN:
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_BEGIN);
                break;
            case BRU_ACT_END:
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_END);
                break;

            case BRU_ACT_CHAR:
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_CHAR,
                                 .ch = act->ch);
                break;

            case BRU_ACT_PRED:
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_PRED,
                                 .pred = act->pred);
                break;

            case BRU_ACT_SAVE:
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_SAVE,
                                 .idx = act->k);
                break;

            case BRU_ACT_BACKREF:
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_BACKREF,
                                 .idx = act->k);
                break;

            case BRU_ACT_INC:
                GET_IDX(&mmaps->thread_cmap, mmaps->next_thread_cidx, 1,
                        act->k);
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_INC, .idx = idx);
                break;

            case BRU_ACT_SET:
                GET_IDX(&mmaps->thread_cmap, mmaps->next_thread_cidx, 1,
                        act->k);
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_SET, .idx = idx,
                                 .val = act->val);
                break;

            case BRU_ACT_CMP:
                GET_IDX(&mmaps->thread_cmap, mmaps->next_thread_cidx, 1,
                        act->k);
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_CMP, .idx = idx,
                                 .val = act->val, .ord = act->ord);
                break;

            case BRU_ACT_EPSSET:
                GET_IDX(&mmaps->thread_mmap, mmaps->next_thread_midx,
                        sizeof(const char *), act->k);
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_EPSSET,
                                 .idx = idx);
                break;

            case BRU_ACT_EPSCHK:
                GET_IDX(&mmaps->thread_mmap, mmaps->next_thread_midx,
                        sizeof(const char *), act->k);
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_EPSCHK,
                                 .idx = idx);
                break;

            case BRU_ACT_MEMOSET:
                GET_IDX(&mmaps->memo_map, mmaps->next_memo_idx, 1, act->k);
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_MEMOSET,
                                 .idx = idx);
                // NOTE: MEMOSET is a sink instruction that will kill any thread
                // that runs it, so we do not need to compile any other actions
                // since the thread will be killed.
                // *continue_compilation = false;
                // goto done;
                break;

            case BRU_ACT_MEMOCHK:
                GET_IDX(&mmaps->memo_map, mmaps->next_memo_idx, 1, act->k);
                PUSH_INSTRUCTION(instructions, .bytecode = BRU_MEMOCHK,
                                 .idx = idx);
                break;

            case BRU_ACT_WRITE:
                switch (act->c) {
                    case '0':
                        PUSH_INSTRUCTION(instructions, .bytecode = BRU_WRITE0);
                        break;
                    case '1':
                        PUSH_INSTRUCTION(instructions, .bytecode = BRU_WRITE1);
                        break;
                    default:
                        PUSH_INSTRUCTION(instructions, .bytecode = BRU_WRITE,
                                         .c = act->c);
                        break;
                }
                break;

            case BRU_ACT_NACTIONS: assert(false && "unreachable"); break;
        }
    }

    bru_smir_action_list_iter_free(iter);

#undef GET_IDX
}

static void compile_transition(BruStateMachine        *sm,
                               StcVec(BruInstruction) *instructions,
                               bru_trans_id            tid,
                               BruBackpatch          **state_patches,
                               BruMemoryMaps          *mmaps)
{
    const BruActionList *acts;
    BruBackpatch        *jmp_patch;
    bru_state_id         dst;

    acts = bru_smir_trans_get_actions(sm, tid);

    compile_actions(instructions, acts, mmaps);

    dst = bru_smir_get_dst(sm, tid);
    MAKE_PATCH(jmp_patch, next = *state_patches,
               state_id        = BRU_IS_FINAL_STATE(dst)
                                     ? bru_smir_get_num_states(sm) + 1
                                     : dst,
               patch_instr_idx = CURRENT_INSTRUCTION);
    *state_patches = jmp_patch;
    PUSH_INSTRUCTION(instructions, .bytecode = BRU_JMP);
}

static void compile_state(BruStateMachine        *sm,
                          StcVec(BruInstruction) *instructions,
                          bru_state_id            sid,
                          bru_compile_f          *pre,
                          bru_compile_f          *post,
                          BruBackpatch          **transition_patches,
                          BruBackpatch          **state_patches,
                          size_t                 *state_starts,
                          BruMemoryMaps          *mmaps)
{
    BruBackpatch        *tmp_patch, *head, *tail;
    size_t               transition_instruction_idx;
    bru_trans_id        *out = NULL;
    const BruActionList *acts;
    size_t               i, n;

    state_starts[sid] = CURRENT_INSTRUCTION;

    if (pre) pre(bru_smir_get_pre_meta(sm, sid), instructions);
    if (bru_smir_state_get_num_actions(sm, sid)) {
        acts = bru_smir_state_get_actions(sm, sid);
        compile_actions(instructions, acts, mmaps);
    }
    if (post) post(bru_smir_get_post_meta(sm, sid), instructions);

    out                        = bru_smir_get_out_transitions(sm, sid, &n);
    transition_instruction_idx = CURRENT_INSTRUCTION;
    switch (n) {
        case 0: goto done;
        case 1: PUSH_INSTRUCTION(instructions, .bytecode = BRU_JMP); break;
        case 2: PUSH_INSTRUCTION(instructions, .bytecode = BRU_SPLIT); break;
        default:
            PUSH_INSTRUCTION(instructions, .bytecode = BRU_TSWITCH,
                             .tswitch = stc_vec_default(BruInstruction *));
            break;
    }

    // create list of transition backpatches and insert into front of list
    // i = 0 handled explicitly to maintain pointer to start of the list (head)
    MAKE_PATCH(tmp_patch, next = *transition_patches,
               patch_instr_idx  = transition_instruction_idx,
               target_instr_idx = CURRENT_INSTRUCTION);
    compile_transition(sm, instructions, out[0], state_patches, mmaps);
    tail = head = tmp_patch;
    for (i = 1; i < n; i++) {
        MAKE_PATCH(tmp_patch, next = *transition_patches,
                   patch_instr_idx  = transition_instruction_idx,
                   target_instr_idx = CURRENT_INSTRUCTION);
        tail->next = tmp_patch;
        tail       = tmp_patch;
        compile_transition(sm, instructions, out[i], state_patches, mmaps);
    }
    *transition_patches = head;

done:
    if (out) free(out);
}

static void compile_initial(BruStateMachine        *sm,
                            StcVec(BruInstruction) *instructions,
                            BruBackpatch          **transition_patches,
                            BruBackpatch          **state_patches,
                            size_t                 *state_start_idxs,
                            BruMemoryMaps          *mmaps)
{
    compile_state(sm, instructions, BRU_INITIAL_STATE_ID, NULL, NULL,
                  transition_patches, state_patches, state_start_idxs, mmaps);
}

static void resolve_backpatches(StcVec(BruInstruction) instructions,
                                BruBackpatch          *bp)
{
    BruBackpatch   *tmp;
    BruInstruction *dst, *instr;

    while (bp) {
        tmp   = bp->next;
        dst   = instructions + bp->target_instr_idx;
        instr = instructions + bp->patch_instr_idx;

        switch (instr->bytecode) {
            case BRU_JMP: instr->jmp = dst; break;
            case BRU_SPLIT:
                if (instr->split_left)
                    instr->split_right = dst;
                else
                    instr->split_left = dst;
                break;
            case BRU_TSWITCH: stc_vec_push_back(&instr->tswitch, dst); break;

            case BRU_NOOP:
            case BRU_MATCH:
            case BRU_BEGIN:
            case BRU_END:
            case BRU_CHAR:
            case BRU_PRED:
            case BRU_GSPLIT:
            case BRU_LSPLIT:
            case BRU_SAVE:
            case BRU_BACKREF:
            case BRU_INC:
            case BRU_SET:
            case BRU_CMP:
            case BRU_EPSRESET:
            case BRU_EPSSET:
            case BRU_EPSCHK:
            case BRU_MEMOSET:
            case BRU_MEMOCHK:
            case BRU_ZWA:
            case BRU_STATE:
            case BRU_WRITE:
            case BRU_WRITE0:
            case BRU_WRITE1:
            case BRU_NBYTECODES: assert(false && "UNREACHABLE");
        }
        free(bp);
        bp = tmp;
    }
}

/* --- API function definitions --------------------------------------------- */

StcVec(BruInstruction) bru_smir_compile(BruStateMachine *sm)
{
    return bru_smir_compile_with_meta(sm, NULL, NULL);
}

StcVec(BruInstruction) bru_smir_compile_with_meta(BruStateMachine *sm,
                                                  bru_compile_f   *pre,
                                                  bru_compile_f   *post)
{
    StcVec(BruInstruction) instructions;
    BruMemoryMaps          mmaps = { 0 };
    size_t                *state_start_idxs;
    BruBackpatch          *state_patches      = NULL;
    BruBackpatch          *transition_patches = NULL;
    BruBackpatch          *tmp;
    size_t                 n, sid;

    stc_vec_default_init(&instructions);
    stc_vec_default_init(&mmaps.memo_map);
    stc_vec_default_init(&mmaps.thread_cmap);
    stc_vec_default_init(&mmaps.thread_mmap);

    n                = bru_smir_get_num_states(sm);
    state_start_idxs = malloc((n + 2) * sizeof(*state_start_idxs));

    // compile all states, storing their starting instructions for backpatching
    compile_initial(sm, &instructions, &transition_patches, &state_patches,
                    state_start_idxs, &mmaps);
    for (sid = 1; sid <= n; sid++)
        compile_state(sm, &instructions, sid, pre, post, &transition_patches,
                      &state_patches, state_start_idxs, &mmaps);

    state_start_idxs[sid] = stc_vec_len(instructions);
    PUSH_INSTRUCTION(&instructions, .bytecode = BRU_MATCH);

    // look up and store state offsets for backpatching, then resolve
    // backpatching
    tmp = state_patches;
    while (tmp) {
        tmp->target_instr_idx = state_start_idxs[tmp->state_id];
        tmp                   = tmp->next;
    }
    resolve_backpatches(instructions, state_patches);
    resolve_backpatches(instructions, transition_patches);

    // cleanup
    stc_vec_free(mmaps.memo_map);
    stc_vec_free(mmaps.thread_cmap);
    stc_vec_free(mmaps.thread_mmap);
    free(state_start_idxs);

    return instructions;
}
