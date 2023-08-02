#include <stdio.h>
#include <stdlib.h>

#include "srvm.h"

void inst_default(Inst *inst, InstKind kind) { inst->kind = kind; }

void inst_pred(Inst *inst, Interval *intervals, size_t len)
{
    inst->kind      = PRED;
    inst->intervals = intervals;
    inst->len       = len;
}

void inst_save(Inst *inst, const char **k)
{
    inst->kind = SAVE;
    inst->k    = k;
}

void inst_jmp(Inst *inst, Inst *x)
{
    inst->kind = JMP;
    inst->x    = x;
}

void inst_split(Inst *inst, Inst *x, Inst *y)
{
    inst->kind = SPLIT;
    inst->x    = x;
    inst->y    = y;
}

void inst_tswitch(Inst *inst, Inst **xs, size_t len)
{
    inst->kind = TSWITCH;
    inst->xs   = xs;
    inst->len  = len;
}

void inst_lswitch(Inst *inst, Lookup *lookups, size_t len)
{
    inst->kind    = LSWITCH;
    inst->lookups = lookups;
    inst->len     = len;
}

int inst_eps(Inst *inst, InstKind kind, size_t *c)
{
    if (kind < EPSSET || kind > EPSSET) { return FALSE; }

    inst->kind = kind;
    inst->c    = c;

    return TRUE;
}

void inst_reset(Inst *inst, size_t *c, size_t val)
{
    inst->kind = RESET;
    inst->c    = c;
    inst->val  = val;
}

void inst_cmp(Inst *inst, size_t *c, size_t val, Order order)
{
    inst->kind  = CMP;
    inst->c     = c;
    inst->val   = val;
    inst->order = order;
}

void inst_zwa(Inst *inst, Inst *x, Inst *y, int pos)
{
    inst->kind = ZWA;
    inst->x    = x;
    inst->y    = y;
    inst->pos  = pos;
}

char *inst_to_str(Inst *inst)
{
    char *s;

    return s;
}

void inst_free(Inst *inst) {}

Program *program(void)
{
    Program *prog = malloc(sizeof(Program));

    return prog;
}

char *program_to_str(Program *prog)
{
    char *s;

    return s;
}

void program_free(Program *prog) {}

int main(void)
{
    printf("srvm\n");
    return 0;
}
