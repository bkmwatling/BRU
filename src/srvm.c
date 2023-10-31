#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STC_UTF_ENABLE_SHORT_NAMES
#include "stc/util/utf.h"

#include "srvm.h"
#include "types.h"
#include "utils.h"

#define BUFSIZE 512

/* --- Instruction ---------------------------------------------------------- */

byte *inst_to_str(char *s, size_t *len, size_t *alloc, byte *pc, byte *aux)
{
    char    *p;
    cntr_t   c;
    offset_t x, y;
    len_t    i, n;

    if (pc == NULL) return NULL;

    switch (*pc++) {
        case MATCH: STR_PUSH(s, *len, *alloc, "match"); break;
        case BEGIN: STR_PUSH(s, *len, *alloc, "begin"); break;
        case END: STR_PUSH(s, *len, *alloc, "end"); break;

        case CHAR:
            MEMPOP(p, pc, char *);
            p = utf8_to_str(p);
            ENSURE_SPACE(s, *len + strlen(p) + 6, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "char %s", p);
            free(p);
            break;

        case PRED:
            MEMPOP(n, pc, len_t);
            MEMPOP(i, pc, len_t);

            p = intervals_to_str((Interval *) (aux + i), n);
            ENSURE_SPACE(s, *len + strlen(p) + 6, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "pred %s", p);
            free(p);
            break;

        case SAVE:
            MEMPOP(n, pc, len_t);
            ENSURE_SPACE(s, *len + 13, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "save " LEN_FMT, n);
            break;

        case JMP:
            MEMPOP(x, pc, offset_t);
            ENSURE_SPACE(s, *len + 12, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "jmp " OFFSET_FMT, x);
            break;

        case SPLIT:
            MEMPOP(x, pc, offset_t);
            MEMPOP(y, pc, offset_t);
            ENSURE_SPACE(s, *len + 23, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len,
                             "split " OFFSET_FMT ", " OFFSET_FMT, x, y);
            break;

        /* TODO: */
        case GSPLIT: break;

        case LSPLIT: break;

        case TSWITCH: break;

        case LSWITCH: break;

        case EPSSET:
            MEMPOP(n, pc, len_t);
            ENSURE_SPACE(s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "epsset " LEN_FMT, n);
            break;

        case EPSCHK:
            MEMPOP(n, pc, len_t);
            ENSURE_SPACE(s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "epschk " LEN_FMT, n);
            break;

        case RESET:
            MEMPOP(i, pc, len_t);
            MEMPOP(c, pc, cntr_t);
            ENSURE_SPACE(s, *len + 23, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len,
                             "reset " LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case CMP:
            MEMPOP(i, pc, len_t);
            MEMPOP(c, pc, cntr_t);
            ENSURE_SPACE(s, *len + 23, *alloc, sizeof(char));

            switch (*pc++) {
                case LT:
                    *len += snprintf(s + *len, *alloc - *len, "cmplt ");
                    break;
                case LE:
                    *len += snprintf(s + *len, *alloc - *len, "cmple ");
                    break;
                case EQ:
                    *len += snprintf(s + *len, *alloc - *len, "cmpeq ");
                    break;
                case NE:
                    *len += snprintf(s + *len, *alloc - *len, "cmpne ");
                    break;
                case GE:
                    *len += snprintf(s + *len, *alloc - *len, "cmpge ");
                    break;
                case GT:
                    *len += snprintf(s + *len, *alloc - *len, "cmpgt ");
                    break;
            }

            *len +=
                snprintf(s + *len, *alloc - *len, LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case INC:
            MEMPOP(i, pc, len_t);
            ENSURE_SPACE(s, *len + 12, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "inc " LEN_FMT, i);
            break;

        case ZWA:
            MEMPOP(x, pc, offset_t);
            MEMPOP(y, pc, offset_t);
            ENSURE_SPACE(s, *len + 24, *alloc, sizeof(char));
            *len +=
                snprintf(s + *len, *alloc - *len,
                         "zwa " OFFSET_FMT ", " OFFSET_FMT ", %d", x, y, *pc++);
            break;
    }

    return pc;
}

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

char *program_to_str(const Program *prog)
{
    size_t len = 0, alloc = BUFSIZE;
    char  *s  = malloc(alloc * sizeof(char));
    len_t  i  = 0;
    byte  *pc = prog->insts;

    while (pc - prog->insts < prog->insts_len) {
        ENSURE_SPACE(s, len + 6, alloc, sizeof(char));
        len += snprintf(s + len, alloc - len, "%3d: ", i++);
        pc   = inst_to_str(s, &len, &alloc, pc, prog->aux);
        STR_PUSH(s, len, alloc, "\n");
    }
    s[len - 1] = '\0';

    return s;
}

void program_free(Program *prog)
{
    free(prog->insts);
    free(prog->aux);
    free(prog->counters);
    free(prog->memory);
    free(prog);
}
