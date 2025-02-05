#ifndef BRU_VM_COMPILER_H
#define BRU_VM_COMPILER_H

#include <bru/vm/program.h>

/* --- Preprocessor macros -------------------------------------------------- */

#define PUSH_INSTRUCTION(instruction_vec_ptr, ...) \
    stc_vec_push_back(*(instruction_vec_ptr),      \
                      ((BruInstruction) { __VA_ARGS__ }))

/* --- Data structures ------------------------------------------------------ */

typedef struct bru_instruction BruInstruction;

struct bru_instruction {
    BruBytecode bytecode;

    union {
        const char         *ch;   /**< bytecode = BRU_CHAR                    */
        const BruIntervals *pred; /**< bytecode = BRU_PRED                    */
        bru_len_t           idx;  /**< bytecode = BRU_SAVE | BRU_INC |
                                                  BRU_SET |  BRU_CMP |
                                                  BRU_EPSCHK | BRU_EPSSET |
                                                  BRU_MEMOSET | BRU_MEMOCHK   */
        char                c;    /**< bytecode = BRU_WRITE                   */

        BruInstruction          *jmp;        /**< bytecode = BRU_JMP          */
        BruInstruction          *split_left; /**< bytecode = BRU_SPLIT        */
        StcVec(BruInstruction *) tswitch;    /**< bytecode = BRU_TSWITCH      */
    };

    union {
        BruInstruction *split_right; /**< bytecode = BRU_SPLIT                */
        bru_cntr_t      val;         /**< bytecode = BRU_SET | BRU_CMP        */
    };

    BruOrd ord; /**< bytecode = BRU_CMP                                       */

    size_t prog_offset; /**< starting byte offset computed during compilation */
};

typedef enum { BRU_OPTIMISE_NONE, BRU_OPTIMISE_FULL } BruOptimisationLevel;

/**
 * Compile a sequence of instructions into a VM program.
 *
 * @param[in] regex        the original regular expression
 * @param[in] instructions the sequence of instructions to compile
 * @param[in] op_level     the level of optimisations to apply
 *
 * @return the compiled SRVM program
 */
BruProgram *bru_compiler_compile(const char            *regex,
                                 StcVec(BruInstruction) instructions,
                                 BruOptimisationLevel   op_level);

#endif /* BRU_VM_COMPILER_H */
