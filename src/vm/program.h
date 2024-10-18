#ifndef BRU_VM_PROGRAM_H
#define BRU_VM_PROGRAM_H

#include <stc/fatp/vec.h>

#include "../re/sre.h"
#include "../types.h"

/**
 * Write a bytecode to instruction byte stream at given PC and move PC to after
 * bytecode.
 *
 * @param[in,out] pc       the PC to write the bytecode to
 * @param[in]     bytecode the bytecode to write to PC
 */
#define BRU_BCWRITE(pc, bytecode) (*(pc)++ = (bytecode))

/**
 * Push a bytecode to the end of an instruction byte stream.
 *
 * @param[in] insts    the instruction byte stream
 * @param[in] bytecode the bytecode to push onto the instruction byte stream
 */
#define BRU_BCPUSH(insts, bytecode) stc_vec_push_back(insts, bytecode)

/**
 * Read the bytecode from PC, and move PC past the bytecode in the underlying
 * byte stream.
 *
 * @param[in,out] pc the PC to read the bytecode from
 */
#define BRU_BCREAD(pc) ((BruBytecode) (*(pc)++))

/**
 * Write a value of given type to PC and advance PC past that value.
 *
 * @param[in,out] pc   the PC to write the value to
 * @param[in]     type the type of the value to write to PC
 * @param[in]     val  the value to write to PC
 */
#define BRU_MEMWRITE(pc, type, val)       \
    do {                                  \
        *((type *) (pc))  = (val);        \
        (pc)             += sizeof(type); \
    } while (0)

/**
 * Push a value of a given type to end of a byte stream.
 *
 * @param[in] bytes the byte stream
 * @param[in] type  the type of the value to push onto the byte stream
 * @param[in] val   the value to push onto the byte stream
 */
#define BRU_MEMPUSH(bytes, type, val)                                      \
    do {                                                                   \
        stc_vec_reserve(bytes, sizeof(type));                              \
        *((type *) ((bytes) + stc_vec_len_unsafe(bytes)))  = (val);        \
        stc_vec_len_unsafe(bytes)                         += sizeof(type); \
    } while (0)

/**
 * Copy a given number of bytes of memory from a given memory address to the end
 * of a byte stream.
 *
 * @param[in] bytes the byte stream
 * @param[in] src   the memory address to copy from
 * @param[in] size  the number of bytes to copy
 */
#define BRU_MEMCPY(bytes, src, size)                                \
    do {                                                            \
        stc_vec_reserve(bytes, size);                               \
        memcpy((bytes) + stc_vec_len_unsafe(bytes), (src), (size)); \
        stc_vec_len_unsafe(bytes) += (size);                        \
    } while (0)

/**
 * Read the value of the given type from PC into the destination, and move PC
 * past the value in the underlying byte stream.
 *
 * @param[in]     dst  the destination to save the value into
 * @param[in,out] pc   the PC to read the value from
 * @param[in]     type the type of the value to read
 */
#define BRU_MEMREAD(dst, pc, type) \
    do {                           \
        (dst)  = *((type *) (pc)); \
        (pc)  += sizeof(type);     \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

typedef enum {
    BRU_NOOP = 0,
    BRU_MATCH,
    BRU_BEGIN,
    BRU_END,
    BRU_MEMO,
    BRU_CHAR,
    BRU_PRED,
    BRU_SAVE,
    BRU_JMP,
    BRU_SPLIT,
    BRU_GSPLIT,
    BRU_LSPLIT,
    BRU_TSWITCH,
    BRU_EPSRESET,
    BRU_EPSSET,
    BRU_EPSCHK,
    BRU_RESET,
    BRU_CMP,
    BRU_INC,
    BRU_ZWA,
    BRU_STATE,
    BRU_WRITE,
    BRU_WRITE0,
    BRU_WRITE1,
    BRU_NBYTECODES
} BruBytecode;

/* Order for comparisons */
typedef enum { BRU_LT = 1, BRU_LE, BRU_EQ, BRU_NE, BRU_GE, BRU_GT } BruOrd;

typedef struct {
    const char *regex; /**< the original regular expression string            */

    // VM execution
    StcVec(bru_byte_t) insts; /**< the instruction byte stream                */
    StcVec(bru_byte_t) aux;   /**< the auxillary memory for the program       */

    // shared thread memory
    size_t nmemo_insts; /**< the number of memoisation instructions           */

    // thread memory
    StcVec(bru_cntr_t) counters; /**< the counter memory default values       */
    size_t thread_mem_len; /**< the number of bytes needed for thread memory  */
    size_t ncaptures;      /**< the number of captures in the program/regex   */

    // compile-time collected info
    int requires_writing; /**< if the program contains WRITE* instructions    */
} BruProgram;

#if !defined(BRU_VM_PROGRAM_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_PROGRAM_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&        \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||     \
          defined(BRU_ENABLE_SHORT_NAMES)))
#    define BCWRITE  BRU_BCWRITE
#    define BCPUSH   BRU_BCPUSH
#    define BCREAD   BRU_BCREAD
#    define MEMWRITE BRU_MEMWRITE
#    define MEMPUSH  BRU_MEMPUSH
#    define MEMCPY   BRU_MEMCPY
#    define MEMREAD  BRU_MEMREAD

typedef BruBytecode Bytecode;
#    define NOOP       BRU_NOOP
#    define MATCH      BRU_MATCH
#    define BEGIN      BRU_BEGIN
#    define END        BRU_END
#    define MEMO       BRU_MEMO
#    define CHAR       BRU_CHAR
#    define PRED       BRU_PRED
#    define SAVE       BRU_SAVE
#    define JMP        BRU_JMP
#    define SPLIT      BRU_SPLIT
#    define GSPLIT     BRU_GSPLIT
#    define LSPLIT     BRU_LSPLIT
#    define TSWITCH    BRU_TSWITCH
#    define EPSRESET   BRU_EPSRESET
#    define EPSSET     BRU_EPSSET
#    define EPSCHK     BRU_EPSCHK
#    define RESET      BRU_RESET
#    define CMP        BRU_CMP
#    define INC        BRU_INC
#    define ZWA        BRU_ZWA
#    define STATE      BRU_STATE
#    define NBYTECODES BRU_NBYTECODES

typedef BruOrd Ord;
#    define LT BRU_LT
#    define LE BRU_LE
#    define EQ BRU_EQ
#    define NE BRU_NE
#    define GE BRU_GE
#    define GT BRU_GT

typedef BruProgram Program;

#    define program_new     bru_program_new
#    define program_default bru_program_default
#    define program_free    bru_program_free
#    define program_print   bru_program_print
#    define inst_print      bru_inst_print
#endif /* BRU_VM_PROGRAM_ENABLE_SHORT_NAMES */

/* --- Program function prototypes ------------------------------------------ */

/**
 * Construct a program with preallocated memory and lengths.
 *
 * NOTE: the instruction stream is not populated, but just allocated.
 * NOTE: values of 0 for lengths of memory (including memoisation, counters, and
 *       captures) mean those pointers are NULL and shouldn't be used.
 *
 * @param[in] regex          original regex string of the program
 * @param[in] insts_len      number of bytes to allocate for instruction stream
 * @param[in] aux_len        number of bytes to allocate for auxillary memory
 * @param[in] nmemo_insts    number of memory instructions in the program
 * @param[in] ncounters      number of counters in the program
 * @param[in] thread_mem_len number of bytes to allocate for thread memory
 * @param[in] ncaptures      number of captures in the program
 *
 * @return the constructed program with preallocated memory and lengths
 */
BruProgram *bru_program_new(const char *regex,
                            size_t      insts_len,
                            size_t      aux_len,
                            size_t      nmemo_insts,
                            size_t      ncounters,
                            size_t      thread_mem_len,
                            size_t      ncaptures);

/**
 * Construct a default program without preallocating memory and lengths.
 *
 * @param[in] regex the original regex string of the program
 *
 * @return the constructed program without preallocated memory and lengths
 */
BruProgram *bru_program_default(const char *regex);

/**
 * Free the memory allocated for the program (including the memory of the regex
 * string).
 *
 * @param[in] self the program to free
 */
void bru_program_free(BruProgram *self);

/**
 * Print the instruction stream of the program in human readable format.
 *
 * @param[in] self   the program to print instruction stream of
 * @param[in] stream the file stream to print to
 */
void bru_program_print(const BruProgram *self, FILE *stream);

/**
 * Print an instruction to the file stream, with its operands.
 *
 * @param[in] stream the file stream
 * @param[in] pc     the pointer into the instruction stream
 */
void bru_inst_print(FILE *stream, const bru_byte_t *pc);

#endif /* BRU_VM_PROGRAM_H */
