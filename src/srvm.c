#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STC_UTF_ENABLE_SHORT_NAMES
#include "stc/util/utf.h"

#include "srvm.h"
#include "types.h"
#include "utils.h"

#define BUFSIZE 512

/* --- Helper function prototypes ------------------------------------------- */

static len_t
offset_to_absolute_index(offset_t x, const byte *pc, const byte *insts);
static const byte *inst_to_str(char          *s,
                               size_t        *len,
                               size_t        *alloc,
                               const byte    *pc,
                               const Program *prog);

/* --- Program -------------------------------------------------------------- */

Program *program_new(len_t insts_size,
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
    size_t      len = 0, alloc = BUFSIZE;
    char       *s  = malloc(alloc * sizeof(char));
    len_t       i  = 0;
    const byte *pc = prog->insts;

    while (pc - prog->insts < prog->insts_len) {
        ENSURE_SPACE(s, len + 6, alloc, sizeof(char));
        len += snprintf(s + len, alloc - len, "%3d: ", i++);
        pc   = inst_to_str(s, &len, &alloc, pc, prog);
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

/* --- Helper function definitions ------------------------------------------ */

static len_t
offset_to_absolute_index(offset_t x, const byte *pc, const byte *insts)
{
    len_t idx;

    pc += x;
    for (idx = 0; insts != pc; idx++) {
        switch (*insts++) {
            case CHAR: insts += sizeof(char *); break;
            case PRED: insts += 2 * sizeof(len_t); break;
            case SAVE: insts += sizeof(len_t); break;
            case JMP:    /* fallthrough */
            case GSPLIT: /* fallthrough */
            case LSPLIT: insts += sizeof(offset_t); break;
            case SPLIT: insts += 2 * sizeof(offset_t); break;
            case TSWITCH: break; /* TODO: */
            case LSWITCH: break; /* TODO: */
            case EPSSET:         /* fallthrough */
            case EPSCHK: insts += sizeof(len_t); break;
            case RESET: insts += sizeof(len_t) + sizeof(cntr_t); break;
            case CMP: insts += 1 + sizeof(len_t) + sizeof(cntr_t); break;
            case INC: insts += sizeof(cntr_t); break;
            case ZWA: insts += 1 + 2 * sizeof(offset_t); break;
            default: break;
        }
    }

    return idx;
}

static const byte *inst_to_str(char          *s,
                               size_t        *len,
                               size_t        *alloc,
                               const byte    *pc,
                               const Program *prog)
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
            ENSURE_SPACE(s, *len + strlen(p) + 6, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "char %.*s",
                             utf8_nbytes(p), p);
            break;

        case PRED:
            MEMPOP(n, pc, len_t);
            MEMPOP(i, pc, len_t);

            p = intervals_to_str((Interval *) (prog->aux + i), n);
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
            *len += snprintf(s + *len, *alloc - *len, "jmp " LEN_FMT,
                             offset_to_absolute_index(x, pc - sizeof(offset_t),
                                                      prog->insts));
            break;

        case SPLIT:
            MEMPOP(x, pc, offset_t);
            MEMPOP(y, pc, offset_t);
            ENSURE_SPACE(s, *len + 23, *alloc, sizeof(char));
            *len +=
                snprintf(s + *len, *alloc - *len, "split " LEN_FMT ", " LEN_FMT,
                         offset_to_absolute_index(x, pc - 2 * sizeof(offset_t),
                                                  prog->insts),
                         offset_to_absolute_index(y, pc - sizeof(offset_t),
                                                  prog->insts));
            break;

        case GSPLIT:
            MEMPOP(x, pc, offset_t);
            ENSURE_SPACE(s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "gsplit " LEN_FMT,
                             offset_to_absolute_index(x, pc - sizeof(offset_t),
                                                      prog->insts));
            break;

        case LSPLIT:
            MEMPOP(x, pc, offset_t);
            ENSURE_SPACE(s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "lsplit " LEN_FMT,
                             offset_to_absolute_index(x, pc - sizeof(offset_t),
                                                      prog->insts));
            break;

        /* TODO: */
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
            *len += snprintf(
                s + *len, *alloc - *len, "zwa " LEN_FMT ", " LEN_FMT ", %d",
                offset_to_absolute_index(x, pc - 2 * sizeof(offset_t),
                                         prog->insts),
                offset_to_absolute_index(y, pc - sizeof(offset_t), prog->insts),
                *pc);
            pc++;
            break;
    }

    return pc;
}
