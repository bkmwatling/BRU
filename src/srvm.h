#ifndef SRVM_H
#define SRVM_H

#include "sre.h"

// #define MEMSET(pc, type, val) *((*((type **) &(pc)))++) = (val)
#define MEMSET(pc, type, val)  \
    *((type *) (pc))  = (val); \
    (pc)             += sizeof(type);

/* --- Type definitions ----------------------------------------------------- */

/* Bytecodes */
#define NOOP    0
#define MATCH   1
#define BEGIN   2
#define END     3
#define PRED    4
#define SAVE    5
#define JMP     6
#define SPLIT   7
#define GSPLIT  8
#define LSPLIT  9
#define TSWITCH 10
#define LSWITCH 11
#define EPSSET  12
#define EPSCHK  13
#define RESET   14
#define CMP     15
#define INC     16
#define ZWA     17

typedef struct inst_s Inst;

typedef enum {
    LT, /* less than */
    LE, /* less than or equal */
    EQ, /* equal */
    NE, /* not equal */
    GE, /* greater than or equal */
    GT, /* greater than */
} Order;

typedef struct {
    Inst     *x;
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

/* --- Instruction function prototypes -------------------------------------- */

char *inst_to_str(byte *pc);
void  inst_free(byte *pc);

/* --- Program function prototypes ------------------------------------------ */

Program *program(len_t insts_size,
                 len_t aux_size,
                 len_t grp_cnt,
                 len_t counters_len,
                 len_t mem_len);

char *program_to_str(Program *prog);
void  program_free(Program *prog);

#endif /* SRVM_H */
