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

void  inst_default(Inst *inst, InstKind kind);
void  inst_pred(Inst *inst, Interval *intervals, size_t len);
void  inst_save(Inst *inst, const char **k);
void  inst_jmp(Inst *inst, Inst *x);
void  inst_split(Inst *inst, Inst *x, Inst *y);
void  inst_tswitch(Inst *inst, Inst **xs, size_t len);
void  inst_lswitch(Inst *inst, Lookup *lookups, size_t len);
int   inst_eps(Inst *inst, InstKind kind, size_t *c);
void  inst_reset(Inst *inst, size_t *c, size_t val);
void  inst_cmp(Inst *inst, size_t *c, size_t val, Order order);
void  inst_zwa(Inst *inst, Inst *x, Inst *y, int pos);
char *inst_to_str(Inst *inst);
void  inst_free(Inst *inst);

Program *program(void);
char    *program_to_str(Program *prog);
void     program_free(Program *prog);

#endif /* SRVM_H */
