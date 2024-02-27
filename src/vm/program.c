#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../stc/fatp/vec.h"
#include "../stc/util/utf.h"

#include "../types.h"
#include "program.h"

#define BUFSIZE 512

/* --- Helper function prototypes ------------------------------------------- */

static len_t
offset_to_absolute_index(offset_t x, const byte *pc, const byte *insts);
static const byte *
inst_print(FILE *stream, const byte *pc, const Program *prog);

/* --- API function definitions --------------------------------------------- */

Program *program_new(const char *regex,
                     size_t      insts_len,
                     size_t      aux_len,
                     size_t      nmemo_insts,
                     size_t      ncounters,
                     size_t      thread_mem_len,
                     size_t      ncaptures)
{
    Program *prog = calloc(1, sizeof(*prog));

    prog->regex = regex;
    stc_vec_init(prog->insts, insts_len);
    stc_vec_init(prog->aux, aux_len);
    prog->nmemo_insts = nmemo_insts;
    stc_vec_init(prog->counters, ncounters);
    prog->thread_mem_len = thread_mem_len;
    prog->ncaptures      = ncaptures;

    memset(prog->insts, 0, insts_len * sizeof(byte));
    memset(prog->aux, 0, aux_len * sizeof(byte));
    memset(prog->counters, 0, ncounters * sizeof(cntr_t));

    return prog;
}

Program *program_default(const char *regex)
{
    Program *prog = calloc(1, sizeof(*prog));

    prog->regex = regex;
    stc_vec_default_init(prog->insts);
    stc_vec_default_init(prog->aux);
    stc_vec_default_init(prog->counters);

    memset(prog->insts, 0, STC_VEC_DEFAULT_CAP * sizeof(byte));
    memset(prog->aux, 0, STC_VEC_DEFAULT_CAP * sizeof(byte));
    memset(prog->counters, 0, STC_VEC_DEFAULT_CAP * sizeof(cntr_t));

    return prog;
}

void program_free(Program *self)
{
    free((void *) self->regex);
    stc_vec_free(self->insts);
    stc_vec_free(self->aux);
    stc_vec_free(self->counters);
    free(self);
}

void program_print(const Program *self, FILE *stream)
{
    len_t       i         = 0;
    const byte *pc        = self->insts,
               *insts_end = pc + stc_vec_len_unsafe(self->insts);

    while (pc < insts_end) {
        fprintf(stream, "%4d: ", i++);
        pc = inst_print(stream, pc, self);
        fputc('\n', stream);
    }
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

static const byte *inst_print(FILE *stream, const byte *pc, const Program *prog)
{
    char    *p;
    cntr_t   c;
    offset_t x, y;
    len_t    i, n;

    if (pc == NULL) return NULL;

    switch (*pc++) {
        case NOOP: fputs("noop", stream); break;
        case MATCH: fputs("match", stream); break;
        case BEGIN: fputs("begin", stream); break;
        case END: fputs("end", stream); break;

        case MEMO:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "memo " LEN_FMT, n);
            break;

        case CHAR:
            MEMREAD(p, pc, char *);
            fprintf(stream, "char %.*s", stc_utf8_nbytes(p), p);
            break;

        case PRED:
            MEMREAD(n, pc, len_t);
            MEMREAD(i, pc, len_t);

            p = intervals_to_str((Interval *) (prog->aux + i), n);
            fprintf(stream, "pred %s", p);
            free(p);
            break;

        case SAVE:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "save " LEN_FMT, n);
            break;

        case JMP:
            MEMREAD(x, pc, offset_t);
            fprintf(stream, "jmp " LEN_FMT,
                    offset_to_absolute_index(x, pc, prog->insts));
            break;

        case SPLIT:
            MEMREAD(x, pc, offset_t);
            MEMREAD(y, pc, offset_t);
            fprintf(
                stream, "split " LEN_FMT ", " LEN_FMT,
                offset_to_absolute_index(x, pc - sizeof(offset_t), prog->insts),
                offset_to_absolute_index(y, pc, prog->insts));
            break;

        case GSPLIT:
            MEMREAD(x, pc, offset_t);
            fprintf(stream, "gsplit " LEN_FMT,
                    offset_to_absolute_index(x, pc, prog->insts));
            break;

        case LSPLIT:
            MEMREAD(x, pc, offset_t);
            fprintf(stream, "lsplit " LEN_FMT,
                    offset_to_absolute_index(x, pc, prog->insts));
            break;

        case TSWITCH:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "tswitch " LEN_FMT, n);
            for (i = 0; i < n; i++) {
                MEMREAD(x, pc, offset_t);
                fprintf(stream, ", " LEN_FMT,
                        offset_to_absolute_index(x, pc, prog->insts));
            }
            break;

        case EPSRESET:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "epsreset " LEN_FMT, n);
            break;

        case EPSSET:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "epsset " LEN_FMT, n);
            break;

        case EPSCHK:
            MEMREAD(n, pc, len_t);
            fprintf(stream, "epschk " LEN_FMT, n);
            break;

        case RESET:
            MEMREAD(i, pc, len_t);
            MEMREAD(c, pc, cntr_t);
            fprintf(stream, "reset " LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case CMP:
            MEMREAD(i, pc, len_t);
            MEMREAD(c, pc, cntr_t);

            switch (*pc++) {
                case LT: fputs("cmplt ", stream); break;
                case LE: fputs("cmple ", stream); break;
                case EQ: fputs("cmpeq ", stream); break;
                case NE: fputs("cmpne ", stream); break;
                case GE: fputs("cmpge ", stream); break;
                case GT: fputs("cmpgt ", stream); break;
            }

            fprintf(stream, LEN_FMT ", " CNTR_FMT, i, c);
            break;

        case INC:
            MEMREAD(i, pc, len_t);
            fprintf(stream, "inc " LEN_FMT, i);
            break;

        case ZWA:
            MEMREAD(x, pc, offset_t);
            MEMREAD(y, pc, offset_t);
            fprintf(
                stream, "zwa " LEN_FMT ", " LEN_FMT ", %d",
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
