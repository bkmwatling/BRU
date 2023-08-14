#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srvm.h"

/* --- Instruction ---------------------------------------------------------- */

void inst_default(Inst *inst, InstType type) { inst->type = type; }

void inst_pred(Inst *inst, Interval *intervals, size_t len)
{
    inst->type = PRED;
    inst->len  = len;
    memcpy(inst->intervals, intervals, len * sizeof(Interval));
}

void inst_save(Inst *inst, const char **k)
{
    inst->type = SAVE;
    inst->k    = k;
}

void inst_jmp(Inst *inst, Inst *x)
{
    inst->type = JMP;
    inst->x    = x;
}

void inst_split(Inst *inst, Inst *x, Inst *y)
{
    inst->type = SPLIT;
    inst->x    = x;
    inst->y    = y;
}

void inst_tswitch(Inst *inst, Inst **xs, size_t len)
{
    inst->type = TSWITCH;
    inst->len  = len;
    memcpy(inst->xs, xs, len * sizeof(Inst));
}

void inst_lswitch(Inst *inst, Lookup *lookups, size_t len)
{
    inst->type = LSWITCH;
    inst->len  = len;
    memcpy(inst->lookups, lookups, len * sizeof(Lookup));
}

int inst_eps(Inst *inst, InstType type, size_t *c)
{
    if (type < EPSSET || type > EPSSET) { return FALSE; }

    inst->type = type;
    inst->c    = c;

    return TRUE;
}

void inst_reset(Inst *inst, size_t *c, size_t val)
{
    inst->type = RESET;
    inst->c    = c;
    inst->val  = val;
}

void inst_cmp(Inst *inst, size_t *c, size_t val, Order order)
{
    inst->type  = CMP;
    inst->c     = c;
    inst->val   = val;
    inst->order = order;
}

void inst_zwa(Inst *inst, Inst *x, Inst *y, int pos)
{
    inst->type = ZWA;
    inst->x    = x;
    inst->y    = y;
    inst->pos  = pos;
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

/* --- Main execution ------------------------------------------------------- */

int main(void)
{
    printf("srvm\n");
    return 0;
}
