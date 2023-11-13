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
#define NOOP    0
#define MATCH   1
#define BEGIN   2
#define END     3
#define CHAR    4
#define PRED    5
#define SAVE    6
#define JMP     7
#define SPLIT   8
#define GSPLIT  9
#define LSPLIT  10
#define TSWITCH 11
#define LSWITCH 12
#define EPSSET  13
#define EPSCHK  14
#define RESET   15
#define CMP     16
#define INC     17
#define ZWA     18

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
