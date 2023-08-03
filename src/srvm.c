#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srvm.h"

/* --- Lookup --------------------------------------------------------------- */

size_t lookup_next(Lookup **lookup)
{
    size_t size = sizeof(Lookup) + ((*lookup)->len - 1) * sizeof(Interval);
    char **p    = (char **) lookup;

    *p += size;
    return size;
}

/* --- Instruction ---------------------------------------------------------- */

void inst_default(Inst *inst, InstKind kind) { inst->kind = kind; }

void inst_pred(Inst *inst, Interval *intervals, size_t len)
{
    inst->kind = PRED;
    inst->len  = len;
    memcpy(inst->intervals, intervals, len * sizeof(Interval));
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
    inst->len  = len;
    memcpy(inst->xs, xs, len * sizeof(Inst));
}

void inst_lswitch(Inst *inst, Lookup *lookups, size_t len)
{
    inst->kind = LSWITCH;
    inst->len  = len;
    memcpy(inst->lookups, lookups, len * sizeof(Lookup));
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

size_t inst_next(Inst **inst)
{
    size_t  size, i;
    Lookup *lookup;
    char  **p = (char **) inst;

    switch ((*inst)->kind) {
        case PRED:
            size = sizeof(Inst) + ((*inst)->len - 1) * sizeof(Interval);
            break;
        case TSWITCH:
            size = sizeof(Inst) + ((*inst)->len - 1) * sizeof(Inst *);
            break;
        case LSWITCH:
            size   = sizeof(Inst) - sizeof(Lookup);
            lookup = (*inst)->lookups;
            for (i = 0; i < (*inst)->len; ++i) { size += lookup_next(&lookup); }
            break;
        default: size = sizeof(Inst);
    }
    *p += size;

    return size;
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
