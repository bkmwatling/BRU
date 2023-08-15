#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srvm.h"

/* --- Instruction ---------------------------------------------------------- */

char *inst_to_str(byte *pc)
{
    char *s = NULL;

    return s;
}

void inst_free(byte *pc) {}

/* --- Program -------------------------------------------------------------- */

Program *program(len_t insts_size,
                 len_t aux_size,
                 len_t grp_cnt,
                 len_t counters_len,
                 len_t mem_len)
{
    Program *prog = malloc(sizeof(Program));

    prog->insts     = malloc(insts_size);
    prog->insts_len = insts_size;
    prog->aux       = malloc(aux_size);
    prog->aux_len   = aux_size;
    prog->grp_cnt   = grp_cnt;

    prog->counters     = malloc(counters_len * sizeof(cntr_t));
    prog->counters_len = counters_len;
    prog->memory       = malloc(mem_len * sizeof(size_t));
    prog->mem_len      = mem_len;

    memset(prog->counters, 0, counters_len * sizeof(cntr_t));
    memset(prog->memory, 0, mem_len * sizeof(cntr_t));

    return prog;
}

char *program_to_str(Program *prog)
{
    char *s = NULL;

    return s;
}

void program_free(Program *prog) {}
