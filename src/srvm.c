#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srvm.h"

/* --- Instruction ---------------------------------------------------------- */

void inst_default(Inst *inst, Bytecode bytecode) { inst->bytecode = bytecode; }

void inst_pred(Inst *inst, Interval *intervals, size_t len)
{
    inst->bytecode = PRED;
    inst->len      = len;
    memcpy(inst->intervals, intervals, len * sizeof(Interval));
}

void inst_save(Inst *inst, const char **k)
{
    inst->bytecode = SAVE;
    inst->k        = k;
}

void inst_jmp(Inst *inst, Inst *x)
{
    inst->bytecode = JMP;
    inst->x        = x;
}

void inst_split(Inst *inst, Inst *x, Inst *y)
{
    inst->bytecode = SPLIT;
    inst->x        = x;
    inst->y        = y;
}

void inst_tswitch(Inst *inst, Inst **xs, size_t len)
{
    inst->bytecode = TSWITCH;
    inst->len      = len;
    memcpy(inst->xs, xs, len * sizeof(Inst));
}

void inst_lswitch(Inst *inst, Lookup *lookups, size_t len)
{
    inst->bytecode = LSWITCH;
    inst->len      = len;
    memcpy(inst->lookups, lookups, len * sizeof(Lookup));
}

int inst_eps(Inst *inst, Bytecode bytecode, size_t *n)
{
    if (bytecode < EPSSET || bytecode > EPSSET) { return FALSE; }

    inst->bytecode = bytecode;
    inst->n        = n;

    return TRUE;
}

void inst_reset(Inst *inst, cntr_t *c, cntr_t val)
{
    inst->bytecode = RESET;
    inst->c        = c;
    inst->val      = val;
}

void inst_cmp(Inst *inst, cntr_t *c, cntr_t val, Order order)
{
    inst->bytecode = CMP;
    inst->c        = c;
    inst->val      = val;
    inst->order    = order;
}

void inst_zwa(Inst *inst, Inst *x, Inst *y, int pos)
{
    inst->bytecode = ZWA;
    inst->x        = x;
    inst->y        = y;
    inst->pos      = pos;
}

char *inst_to_str(Inst *inst)
{
    char *s = NULL;

    return s;
}

void inst_free(Inst *inst) {}

/* --- Program -------------------------------------------------------------- */

Program *program(void)
{
    Program *prog = malloc(sizeof(Program));

    return prog;
}

char *program_to_str(Program *prog)
{
    char *s = NULL;

    return s;
}

void program_free(Program *prog) {}
