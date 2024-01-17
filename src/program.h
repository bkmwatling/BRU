#ifndef PROGRAM_H
#define PROGRAM_H

#include "sre.h"
#include "types.h"

#define MEMWRITE(pc, type, val) \
    *((type *) (pc))  = (val);  \
    (pc)             += sizeof(type);

#define MEMREAD(dst, pc, type) \
    (dst)  = *((type *) (pc)); \
    (pc)  += sizeof(type);

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
    byte   *insts;
    len_t   insts_len;
    byte   *aux;
    len_t   aux_len;
    len_t   ncaptures;
    cntr_t *counters;
    len_t   ncounters;
    byte   *memory;
    len_t   mem_len;
} Program;

/* --- Program function prototypes ------------------------------------------ */

Program *program_new(len_t insts_len,
                     len_t aux_len,
                     len_t ncaptures,
                     len_t ncounters,
                     len_t mem_len);
void     program_free(Program *self);
char    *program_to_str(const Program *self);

#endif /* PROGRAM_H */
