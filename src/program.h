#ifndef PROGRAM_H
#define PROGRAM_H

#include "sre.h"
#include "types.h"

// #define MEMPUSH(pc, type, val) *((*((type **) &(pc)))++) = (val)
#define MEMPUSH(pc, type, val) \
    *((type *) (pc))  = (val); \
    (pc)             += sizeof(type);

#define MEMPOP(dst, pc, type)  \
    (dst)  = *((type *) (pc)); \
    (pc)  += sizeof(type);

/* --- Type definitions ----------------------------------------------------- */

/* Bytecodes */
#define NOOP    0
#define START   1
#define MSTART  2
#define MATCH   3
#define BEGIN   4
#define END     5
#define CHAR    6
#define NCHAR   7
#define MCHAR   8
#define PRED    9
#define NPRED   10
#define MPRED   11
#define SAVE    12
#define JMP     13
#define SPLIT   14
#define GSPLIT  15
#define LSPLIT  16
#define TSWITCH 17
#define LSWITCH 18
#define EPSSET  19
#define EPSCHK  20
#define RESET   21
#define CMP     22
#define INC     23
#define ZWA     24

/* Order for cmp */
#define LT 1
#define LE 2
#define EQ 3
#define NE 4
#define GE 5
#define GT 6

typedef struct {
    byte     *x;
    len_t     len;
    Interval *intervals;
} Lookup;

typedef struct {
    byte   *insts;
    len_t   insts_len;
    byte   *aux;
    len_t   aux_len;
    len_t   ncaptures;
    cntr_t *counters;
    len_t   counters_len;
    byte   *memory;
    len_t   mem_len;
} Program;

/* --- Program function prototypes ------------------------------------------ */

Program *program_new(len_t insts_size,
                     len_t aux_size,
                     len_t ncaptures,
                     len_t counters_len,
                     len_t mem_len);
void     program_free(Program *self);
char    *program_to_str(const Program *self);

#endif /* PROGRAM_H */
