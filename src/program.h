#ifndef PROGRAM_H
#define PROGRAM_H

#include "stc/fatp/vec.h"

#include "sre.h"
#include "types.h"

#define BCWRITE(pc, bytecode) *(pc)++ = (bytecode)

#define BCPUSH(insts, bytecode) stc_vec_push_back(insts, bytecode)

#define MEMWRITE(pc, type, val)           \
    do {                                  \
        *((type *) (pc))  = (val);        \
        (pc)             += sizeof(type); \
    } while (0)

#define MEMPUSH(bytes, type, val)                                          \
    do {                                                                   \
        stc_vec_reserve(bytes, sizeof(type));                              \
        *((type *) ((bytes) + stc_vec_len_unsafe(bytes)))  = (val);        \
        stc_vec_len_unsafe(bytes)                         += sizeof(type); \
    } while (0)

#define MEMCPY(bytes, val, size)                                  \
    do {                                                          \
        stc_vec_reserve(bytes, size);                             \
        memcpy((bytes) + stc_vec_len_unsafe(bytes), (val), size); \
        stc_vec_len_unsafe(bytes) += size;                        \
    } while (0)

#define MEMREAD(dst, pc, type)     \
    do {                           \
        (dst)  = *((type *) (pc)); \
        (pc)  += sizeof(type);     \
    } while (0)

/* --- Type definitions ----------------------------------------------------- */

/* Bytecodes */
#define NOOP       0
#define MATCH      1
#define BEGIN      2
#define END        3
#define MEMO       4
#define CHAR       5
#define PRED       6
#define SAVE       7
#define JMP        8
#define SPLIT      9
#define GSPLIT     10
#define LSPLIT     11
#define TSWITCH    12
#define EPSRESET   13
#define EPSSET     14
#define EPSCHK     15
#define RESET      16
#define CMP        17
#define INC        18
#define ZWA        19
#define NBYTECODES 20

/* Order for cmp */
#define LT 1
#define LE 2
#define EQ 3
#define NE 4
#define GE 5
#define GT 6

typedef struct {
    const char *regex;    /*<< the original regular expression                */
    byte       *insts;    /*<< stc_vec                                        */
    byte       *aux;      /*<< stc_vec                                        */
    cntr_t     *counters; /*<< stc_vec                                        */
    byte       *memory;   /*<< stc_vec                                        */
    size_t      ncaptures;
} Program;

/* --- Program function prototypes ------------------------------------------ */

/* NOTE: values of 0 mean those pointers are NULL and shouldn't be used */
Program *program_new(const char *regex,
                     size_t      insts_len,
                     size_t      aux_len,
                     size_t      ncounters,
                     size_t      mem_len,
                     size_t      ncaptures);
Program *program_default(const char *regex);
void     program_free(Program *self);
char    *program_to_str(const Program *self);

#endif /* PROGRAM_H */
