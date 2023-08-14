#ifndef SRVM_H
#define SRVM_H

#include "sre.h"

/* --- Type definitions ----------------------------------------------------- */

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
    size_t    len;
    Interval *intervals;
} Lookup;

typedef enum {
    MATCH,
    BEGIN,
    END,
    PRED,
    SAVE,
    JMP,
    SPLIT,
    GSPLIT,
    LSPLIT,
    TSWITCH,
    LSWITCH,
    EPSSET,
    EPSCHK,
    RESET,
    CMP,
    INC,
    ZWA,
} InstType;

struct inst_s {
    InstType type;

    union {
        Interval    *intervals; /* pointer to interval array (pred) */
        Inst       **xs;        /* pointer to inst pointer array (tswitch) */
        Lookup      *lookups;   /* pointer to lookup array (lswitch) */
        Inst        *x; /* for jumping to an instruction (jmp, split, zwa) */
        const char **k; /* double pointer to update capture info */
        size_t      *c; /* pointer to memory for counters or checks */
    };

    union {
        size_t len; /* for arrays (pred, tswitch, lswitch) */
        size_t val; /* for setting counter (reset) */
        Inst  *y;   /* for jumping to another instruction (split, zwa) */
    };

    union {
        int   pos;   /* for zero-width assertions */
        Order order; /* for cmp */
    };
};

typedef struct {
    Inst   *insts;
    size_t  insts_len;
    char   *aux;
    size_t  aux_len;
    size_t  grp_cnt;
    size_t *counters;
    size_t  counters_len;
    size_t *memory;
    size_t  mem_len;
} Program;

/* --- Instruction function prototypes -------------------------------------- */

void inst_default(Inst *inst, InstType type);
void inst_pred(Inst *inst, Interval *intervals, size_t len);
void inst_save(Inst *inst, const char **k);
void inst_jmp(Inst *inst, Inst *x);
void inst_split(Inst *inst, Inst *x, Inst *y);
void inst_tswitch(Inst *inst, Inst **xs, size_t len);
void inst_lswitch(Inst *inst, Lookup *lookups, size_t len);
int  inst_eps(Inst *inst, InstType type, size_t *c);
void inst_reset(Inst *inst, size_t *c, size_t val);
void inst_cmp(Inst *inst, size_t *c, size_t val, Order order);
void inst_zwa(Inst *inst, Inst *x, Inst *y, int pos);

char *inst_to_str(Inst *inst);
void  inst_free(Inst *inst);

/* --- Program function prototypes ------------------------------------------ */

Program *program(void);

char *program_to_str(Program *prog);
void  program_free(Program *prog);

#endif /* SRVM_H */
