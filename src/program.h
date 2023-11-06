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
#define MCHAR   7
#define PRED    8
#define MPRED   9
#define SAVE    10
#define JMP     11
#define SPLIT   12
#define GSPLIT  13
#define LSPLIT  14
#define TSWITCH 15
#define LSWITCH 16
#define EPSSET  17
#define EPSCHK  18
#define RESET   19
#define CMP     20
#define INC     21
#define ZWA     22

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
    len_t   grp_cnt;
    cntr_t *counters;
    len_t   counters_len;
    mem_t  *memory;
    len_t   mem_len;
} Program;

/* --- Program function prototypes ------------------------------------------ */

Program *program_new(len_t insts_size,
                     len_t aux_size,
                     len_t grp_cnt,
                     len_t counters_len,
                     len_t mem_len);
void     program_free(Program *self);
char    *program_to_str(const Program *self);

#endif /* PROGRAM_H */
