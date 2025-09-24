#ifndef BRU_VM_COMPILERS_SMIR_H
#define BRU_VM_COMPILERS_SMIR_H

#include "../../fa/smir.h"
#include "../compiler.h"

/**
 * A compiler function takes in meta data at states and appends instructions
 * onto the sequence of instructions.
 */
typedef void bru_compile_f(void *meta, StcVec(BruInstruction) *prog);

/**
 * Compile a state machine.
 *
 * @param[in] self the state machine
 *
 * @return the compiled sequence of instructions
 */
StcVec(BruInstruction) bru_smir_compile(BruStateMachine *self);

/**
 * Compile the state machine to VM instructions, including the meta data.
 *
 * The compiler functions can be NULL; corresponding meta data will be ignored.
 *
 * @param[in] self      the state machine
 * @param[in] pre_meta  the compiler for pre-predicate meta data at states
 * @param[in] post_meta the compiler for post-predicate meta data at states
 *
 * @return the compiled sequence of instructions
 */
StcVec(BruInstruction) bru_smir_compile_with_meta(BruStateMachine *self,
                                                  bru_compile_f   *pre_meta,
                                                  bru_compile_f   *post_meta);

#endif /* BRU_VM_COMPILERS_SMIR_H */
