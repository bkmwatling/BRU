#ifndef BRU_VM_COMPILERS_OPTIMISE_H
#define BRU_VM_COMPILERS_OPTIMISE_H

/**
 * @file: optimisation.h
 * @author: Alexander Roodt
 * @brief: A collection of code optimisation routines.
 *
 * Optimisation routines typically consist of (1) deleting some code, or (2)
 * restructuring control flow to be more efficient.
 *
 * In the case of (1), an instruction is deleted by replacing the bytecode
 * with BRU_NOOP. The compiler in `compiler.c` does not compile NOOP
 * instructions, so they are effectively removed from the final program.
 *
 * In the case of (2), a key concept is which instruction follows a given
 * instruction -- termed the 'next logical instruction'. This is affected only
 * by JMP and NOOP instructions. It can be defined recursively:
 *
 *                | next(instr.jmp)     if instr.bytecode = BRU_JMP
 * next(instr) =  | next(instr + 1)     if instr.bytecode = BRU_NOOP
 *                | instr               otherwise
 *
 * Note that other control flow instructions, such as SPLIT, do not get the same
 * treatment. This is due to them having multiple destination instructions.
 *
 * Using this definition, an easy optimisation is to ensure all jump targets go
 * to the next logical instruction.
 */

#include <bru/vm/compiler.h>

/**
 * Eliminates unreachable code by overwriting each instruction with a NOOP.
 *
 * @param[in] instructions the sequence of instructions
 */
void bru_optimise_remove_dead_code(StcVec(BruInstruction) instructions);

/**
 * Update jump targets to point to first non-jump instruction on their path.
 *
 * For example:
 *
 * 0: split 1, 3    ->  split 3, 3
 * 1: jmp 2         ->  jmp 3
 * 2: jmp 3         ->  jmp 3
 * 3: char 'a'      ->  char 'a'
 *
 * This may also create dead code (in the example, instruction 1 and 2).
 *
 * @param[in] instructions the sequence of instructions.
 */
void bru_optimise_compress_control_flow_chain(
    StcVec(BruInstruction) instructions);

/**
 * Replaces jumps with NOOP if it leads to the same instruction.
 *
 * For example:
 *
 *  0: jmp 1    ->  nop
 *  1: char 'a' ->  char 'a'
 *  2: jmp 3    ->  nop
 *  3: nop      ->  nop
 *  4: nop      ->  nop
 *  5: char 'b' ->  char 'b'
 *
 * This optimisation should be the final one run, as it benefits from having
 * NOOPs.
 *
 * @param[in] instructions the sequence of instructions
 */
void bru_optimise_remove_unnecessary_jumps(StcVec(BruInstruction) instructions);

#endif /* BRU_VM_COMPILERS_OPTIMISE_H */
