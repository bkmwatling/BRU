#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stc/util/utf.h"

#include "program.h"
#include "types.h"
#include "utils.h"

#define BUFSIZE 512

/* --- Helper function prototypes ------------------------------------------- */

static len_t
offset_to_absolute_index(offset_t x, const byte *pc, const byte *insts);
static const byte *inst_to_str(char         **s,
                               size_t        *len,
                               size_t        *alloc,
                               const byte    *pc,
                               const Program *prog);

/* --- Program -------------------------------------------------------------- */

Program *program_new(len_t insts_len,
                     len_t aux_len,
                     len_t ncaptures,
                     len_t ncounters,
                     len_t mem_len)
{
    Program *prog = malloc(sizeof(Program));

    prog->insts     = malloc(insts_len);
    prog->insts_len = insts_len;
    prog->aux       = malloc(aux_len);
    prog->aux_len   = aux_len;
    prog->ncaptures = ncaptures;

    prog->counters  = malloc(ncounters * sizeof(cntr_t));
    prog->ncounters = ncounters;
    prog->memory    = malloc(mem_len * sizeof(byte));
    prog->mem_len   = mem_len;

    memset(prog->counters, 0, ncounters * sizeof(cntr_t));
    memset(prog->memory, 0, mem_len * sizeof(byte));

    return prog;
}

void program_free(Program *self)
{
    free(self->insts);
    free(self->aux);
    free(self->counters);
    free(self->memory);
    free(self);
}

char *program_to_str(const Program *self)
{
    size_t      len = 0, alloc = BUFSIZE;
    char       *s  = malloc(alloc * sizeof(char));
    len_t       i  = 0;
    const byte *pc = self->insts;

    while (pc - self->insts < self->insts_len) {
        ENSURE_SPACE(s, len + 6, alloc, sizeof(char));
        len += snprintf(s + len, alloc - len, "%3d: ", i++);
        pc   = inst_to_str(&s, &len, &alloc, pc, self);
        STR_PUSH(s, len, alloc, "\n");
    }
    s[len - 1] = '\0';

    return s;
}

/* --- Helper function definitions ------------------------------------------ */

static len_t
offset_to_absolute_index(offset_t x, const byte *pc, const byte *insts)
{
    len_t idx, len;

    pc += x;
    for (idx = 0; insts < pc; idx++) {
        switch (*insts++) {
            case NOOP:  /* fallthrough */
            case MATCH: /* fallthrough */
            case BEGIN: /* fallthrough */
            case MEMO:  /* fallthrough */
            case END: break;
            case CHAR: insts += sizeof(char *); break;
            case PRED: insts += 2 * sizeof(len_t); break;
            case SAVE: insts += sizeof(len_t); break;
            case JMP:    /* fallthrough */
            case GSPLIT: /* fallthrough */
            case LSPLIT: insts += sizeof(offset_t); break;
            case SPLIT: insts += 2 * sizeof(offset_t); break;
            case TSWITCH:
                MEMREAD(len, insts, len_t);
                insts += len * sizeof(offset_t);
                break;
            case EPSRESET: /* fallthrough */
            case EPSSET:   /* fallthrough */
            case EPSCHK: insts += sizeof(len_t); break;
            case RESET: insts += sizeof(len_t) + sizeof(cntr_t); break;
            case CMP: insts += sizeof(len_t) + sizeof(cntr_t) + 1; break;
            case INC: insts += sizeof(len_t); break;
            case ZWA: insts += 2 * sizeof(offset_t) + 1; break;
            default:
                fprintf(stderr, "bytecode = %d\n", insts[-1]);
                assert(0 && "unreachable");
        }
    }

    return idx;
}

static const byte *inst_to_str(char         **s,
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
        case MATCH: STR_PUSH(*s, *len, *alloc, "match"); break;
        case BEGIN: STR_PUSH(*s, *len, *alloc, "begin"); break;
        case MEMO: STR_PUSH(*s, *len, *alloc, "memoise"); break;
        case END: STR_PUSH(*s, *len, *alloc, "end"); break;

        case CHAR:
            MEMREAD(p, pc, char *);
            ENSURE_SPACE(*s, *len + stc_utf8_nbytes(p) + 6, *alloc,
                         sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "char %.*s",
                             stc_utf8_nbytes(p), p);
            break;

        case PRED:
            MEMREAD(n, pc, len_t);
            MEMREAD(i, pc, len_t);

            p = intervals_to_str((Interval *) (prog->aux + i), n);
            ENSURE_SPACE(*s, *len + strlen(p) + 6, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "pred %s", p);
            free(p);
            break;

        case SAVE:
            MEMREAD(n, pc, len_t);
            ENSURE_SPACE(*s, *len + 13, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "save " LEN_FMT, n);
            break;

        case JMP:
            MEMREAD(x, pc, offset_t);
            ENSURE_SPACE(*s, *len + 12, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "jmp " LEN_FMT,
                             offset_to_absolute_index(x, pc, prog->insts));
            break;

        case SPLIT:
            MEMREAD(x, pc, offset_t);
            MEMREAD(y, pc, offset_t);
            ENSURE_SPACE(*s, *len + 23, *alloc, sizeof(char));
            *len += snprintf(
                *s + *len, *alloc - *len, "split " LEN_FMT ", " LEN_FMT,
                offset_to_absolute_index(x, pc - sizeof(offset_t), prog->insts),
                offset_to_absolute_index(y, pc, prog->insts));
            break;

        case GSPLIT:
            MEMREAD(x, pc, offset_t);
            ENSURE_SPACE(*s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "gsplit " LEN_FMT,
                             offset_to_absolute_index(x, pc, prog->insts));
            break;

        case LSPLIT:
            MEMREAD(x, pc, offset_t);
            ENSURE_SPACE(*s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "lsplit " LEN_FMT,
                             offset_to_absolute_index(x, pc, prog->insts));
            break;

        case TSWITCH:
            MEMREAD(n, pc, len_t);
            ENSURE_SPACE(*s, *len + 16 + 8 * n, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "tswitch " LEN_FMT, n);
            for (i = 0; i < n; i++) {
                MEMREAD(x, pc, offset_t);
                *len += snprintf(*s + *len, *alloc - *len, ", " LEN_FMT,
                                 offset_to_absolute_index(x, pc, prog->insts));
            }
            break;

        case EPSRESET:
            MEMREAD(n, pc, len_t);
            ENSURE_SPACE(*s, *len + 17, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "epsreset " LEN_FMT, n);
            break;

        case EPSSET:
            MEMREAD(n, pc, len_t);
            ENSURE_SPACE(*s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "epsset " LEN_FMT, n);
            break;

        case EPSCHK:
            MEMREAD(n, pc, len_t);
            ENSURE_SPACE(*s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "epschk " LEN_FMT, n);
            break;

        case RESET:
            MEMREAD(i, pc, len_t);
            MEMREAD(c, pc, cntr_t);
            ENSURE_SPACE(*s, *len + 23, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len,
                             "reset " LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case CMP:
            MEMREAD(i, pc, len_t);
            MEMREAD(c, pc, cntr_t);
            ENSURE_SPACE(*s, *len + 23, *alloc, sizeof(char));

            switch (*pc++) {
                case LT:
                    *len += snprintf(*s + *len, *alloc - *len, "cmplt ");
                    break;
                case LE:
                    *len += snprintf(*s + *len, *alloc - *len, "cmple ");
                    break;
                case EQ:
                    *len += snprintf(*s + *len, *alloc - *len, "cmpeq ");
                    break;
                case NE:
                    *len += snprintf(*s + *len, *alloc - *len, "cmpne ");
                    break;
                case GE:
                    *len += snprintf(*s + *len, *alloc - *len, "cmpge ");
                    break;
                case GT:
                    *len += snprintf(*s + *len, *alloc - *len, "cmpgt ");
                    break;
            }

            *len +=
                snprintf(*s + *len, *alloc - *len, LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case INC:
            MEMREAD(i, pc, len_t);
            ENSURE_SPACE(*s, *len + 12, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "inc " LEN_FMT, i);
            break;

        case ZWA:
            MEMREAD(x, pc, offset_t);
            MEMREAD(y, pc, offset_t);
            ENSURE_SPACE(*s, *len + 24, *alloc, sizeof(char));
            *len += snprintf(
                *s + *len, *alloc - *len, "zwa " LEN_FMT ", " LEN_FMT ", %d",
                offset_to_absolute_index(x, pc - sizeof(offset_t), prog->insts),
                offset_to_absolute_index(y, pc, prog->insts), *pc);
            pc++;
            break;

        default:
            fprintf(stderr, "bytecode = %d\n", pc[-1]);
            assert(0 && "unreachable");
    }

    return pc;
}
