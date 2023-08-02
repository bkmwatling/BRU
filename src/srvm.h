#ifndef SRVM_H
#define SRVM_H

#include "sre.h"

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
    Interval *intervals;
    size_t    len;
    Inst     *x;
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
} InstKind;

struct inst_s {
    InstKind kind;

    union {
        Interval    *intervals;
        Inst       **xs;
        Lookup      *lookups;
        Inst        *x;
        const char **k;
        size_t      *c;
    };

    union {
        size_t len;
        size_t val;
        Inst  *y;
    };

    union {
        int   pos;
        Order order;
    };
};

typedef struct {
    Inst   *insts;
    size_t  insts_len;
    size_t  grp_cnt;
    size_t *counters;
    size_t  counters_len;
    size_t *memory;
    size_t  mem_len;
} Program;

Inst  inst_default(InstKind kind);
Inst  inst_pred(Interval *intervals, size_t len);
Inst  inst_save(const char **k);
Inst  inst_jmp(Inst *x);
Inst  inst_split(Inst *x, Inst *y);
Inst  inst_tswitch(Inst **insts, size_t len);
Inst  inst_lswitch(Lookup *lookups, size_t len);
Inst  inst_eps(InstKind kind, size_t *c);
Inst  inst_reset(size_t *c, size_t val);
Inst  inst_cmp(size_t *c, size_t val, Order order);
Inst  inst_zwa(Inst *x, Inst *y, int pos);
char *inst_to_str(Inst *inst);
void  inst_free(Inst *inst);

Program *program(void);
char    *program_to_str(Program *prog);
void     program_free(Program *prog);

#endif /* SRVM_H */
