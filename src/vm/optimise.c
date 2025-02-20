#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <bru/vm/optimise.h>

/* --- Preprocessor macros -------------------------------------------------- */

#define NOP_SLIDE(instr_lvalue)                        \
    do {                                               \
        while ((instr_lvalue)->bytecode == BRU_NOOP && \
               (instr_lvalue) + 1 < instructions + n)  \
            if ((instr_lvalue)->jmp)                   \
                instr_lvalue = (instr_lvalue)->jmp;    \
            else                                       \
                instr_lvalue++;                        \
    } while (0)

#define REMOVE_INSTRUCTION(instruction) \
    memset((instruction), 0, sizeof(*(instruction)))

/* --- Helper function definitions ------------------------------------------ */

static BruInstruction *
next_logical_instruction(StcVec(BruInstruction) instructions,
                         BruInstruction        *instruction)
{
    size_t n = stc_vec_len(instructions);

    NOP_SLIDE(instruction);
    while (instruction->bytecode == BRU_JMP) {
        instruction = instruction->jmp;
        NOP_SLIDE(instruction);
    }

    return instruction;
}

/* --- API function definitions --------------------------------------------- */

void bru_optimise_remove_dead_code(StcVec(BruInstruction) instructions)
{

#define UNEXPLORED(instruction)          \
    ((instruction) < instructions + n && \
     !reachable[(instruction) - instructions])

    size_t                   i, m, n = stc_vec_len(instructions);
    bru_byte_t              *reachable = calloc(n, sizeof(*reachable));
    StcVec(BruInstruction *) dfs_stack;
    BruInstruction          *curr;

    stc_vec_default_init(&dfs_stack);

    stc_vec_push_back(&dfs_stack, instructions);

    while (!stc_vec_is_empty(dfs_stack)) {
        curr                           = stc_vec_pop_back(&dfs_stack);
        reachable[curr - instructions] = TRUE;

        switch (curr->bytecode) {
            case BRU_JMP:
            case BRU_GSPLIT:
            case BRU_LSPLIT:
                if (UNEXPLORED(curr->jmp))
                    stc_vec_push_back(&dfs_stack, curr->jmp);
                break;

            case BRU_SPLIT:
                if (UNEXPLORED(curr->split_left))
                    stc_vec_push_back(&dfs_stack, curr->split_left);
                if (UNEXPLORED(curr->split_right))
                    stc_vec_push_back(&dfs_stack, curr->split_right);
                break;

            case BRU_TSWITCH:
                for (i = 0, m = stc_vec_len(curr->tswitch); i < m; i++)
                    if (UNEXPLORED(curr->tswitch[i]))
                        stc_vec_push_back(&dfs_stack, curr->tswitch[i]);
                break;

            case BRU_MATCH:
            case BRU_MEMOSET: break;

            case BRU_NOOP:
            case BRU_BEGIN:
            case BRU_END:
            case BRU_CHAR:
            case BRU_PRED:
            case BRU_SAVE:
            case BRU_BACKREF:
            case BRU_INC:
            case BRU_SET:
            case BRU_CMP:
            case BRU_EPSRESET:
            case BRU_EPSSET:
            case BRU_EPSCHK:
            case BRU_MEMOCHK:
            case BRU_ZWA:
            case BRU_STATE:
            case BRU_WRITE:
            case BRU_WRITE0:
            case BRU_WRITE1:
                if (UNEXPLORED(curr + 1))
                    stc_vec_push_back(&dfs_stack, curr + 1);
                break;

            case BRU_NBYTECODES: assert(FALSE && "UNREACHABLE"); break;
        }
    }

    for (i = 0; i < n; i++)
        if (!reachable[i]) REMOVE_INSTRUCTION(instructions + i);

    stc_vec_free(dfs_stack);
    free(reachable);

#undef UNEXPLORED
}

// TODO: Compress chained control flow statements into singular control flow
// statements that go immediately to non-control flow statements. For example,
// chained jumps should all jump to the last jump's target. One can also
// compress splits/tswitches similarly, but it will be per operand. If using an
// explicit stack, some method of tracking which operand is being replaced
// currently is necessary. Perhaps simply an array for each instruction storing
// the index I of the operand being worked on currently. Must just ensure the
// correct indices are used.
/**
 * Algorithm pseudocode using recursion:
 *
 * Takes in an instruction pointer, and returns the first non-JMP instruction
 * to be executed if we start at the given instruction.
 *
 * compress_flow_chain(instruction_ptr) -> instruction_ptr
 *
 * if we have seen this instruction_ptr before:
 *  if instruction_ptr is JMP:
 *      return instruction_ptr.jmp
 *  return instruction_ptr
 *
 * mark instruction_ptr as seen
 *
 * if instruction_ptr is JMP:
 *  instruction_ptr.jmp = compress_flow_chain(instruction_ptr.jmp)
 *  return instruction_ptr.jmp
 *
 * if instruction_ptr is SPLIT:
 *  instruction_ptr.split_left = compress_flow_chain(instruction_ptr.split_left)
 *  instruction_ptr.split_right =
 * compress_flow_chain(instruction_ptr.split_right)
 *
 * if instruction_ptr is TSWITCH:
 *  for i in length(instruction_ptr.tswitch):
 *      instruction_ptr[i] = compress_flow_chain(instruction_ptr[i])
 *
 * return instruction_ptr
 *
 */
void bru_optimise_compress_control_flow_chain(
    StcVec(BruInstruction) instructions)
{
    size_t          i, j, m, n = stc_vec_len(instructions);
    BruInstruction *curr;

    for (i = 0, curr = instructions; i < n; i++, curr++) {
        switch (curr->bytecode) {
            case BRU_GSPLIT:
            case BRU_LSPLIT:
            case BRU_JMP:
                curr->jmp = next_logical_instruction(instructions, curr->jmp);
                break;
            case BRU_SPLIT:
                curr->split_left =
                    next_logical_instruction(instructions, curr->split_left);
                curr->split_right =
                    next_logical_instruction(instructions, curr->split_right);
                break;
            case BRU_TSWITCH:
                m = stc_vec_len(curr->tswitch);
                for (j = 0; j < m; j++) {
                    curr->tswitch[j] = next_logical_instruction(
                        instructions, curr->tswitch[j]);
                }
                break;

            case BRU_NOOP:
            case BRU_MATCH:
            case BRU_BEGIN:
            case BRU_END:
            case BRU_CHAR:
            case BRU_PRED:
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
            case BRU_WRITE1: break;

            case BRU_NBYTECODES: assert(FALSE && "UNREACHABLE"); break;
        }
    }
}

void bru_optimise_remove_unnecessary_jumps(StcVec(BruInstruction) instructions)
{
    size_t          i, n = stc_vec_len(instructions);
    BruInstruction *instr;

    for (i = 0, instr = instructions; i < n - 1; i++, instr++)
        if (next_logical_instruction(instructions, instr) ==
            next_logical_instruction(instructions, instr + 1))
            REMOVE_INSTRUCTION(instr);
}
