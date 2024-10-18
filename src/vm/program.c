#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stc/fatp/vec.h>
#include <stc/util/utf.h>

#include "../types.h"
#include "../utils.h"
#include "program.h"

#define BUFSIZE 512

/**< function type for printing an offset to a file stream */
typedef void bru_offset_print_f(FILE             *stream,
                                bru_offset_t      offset,
                                const bru_byte_t *pc,
                                const bru_byte_t *insts);

/**< function type for printing a predicate */
typedef void
bru_predicate_print_f(FILE *stream, bru_len_t idx, const bru_byte_t *aux);

/* --- Helper function prototypes ------------------------------------------- */

static const bru_byte_t *
inst_print_formatted(FILE                  *stream,
                     const bru_byte_t      *pc,
                     const BruProgram      *prog,
                     bru_predicate_print_f *print_predicate,
                     bru_offset_print_f    *print_offset);
static void
print_predicate_as_string(FILE *stream, bru_len_t idx, const bru_byte_t *aux);
static void
print_predicate_as_index(FILE *stream, bru_len_t idx, const bru_byte_t *aux);
static void print_offset_as_absolute_index(FILE             *stream,
                                           bru_offset_t      x,
                                           const bru_byte_t *pc,
                                           const bru_byte_t *insts);
static void print_offset_as_offset(FILE             *stream,
                                   bru_offset_t      offset,
                                   const bru_byte_t *pc,
                                   const bru_byte_t *insts);

/* --- API function definitions --------------------------------------------- */

BruProgram *bru_program_new(const char *regex,
                            size_t      insts_len,
                            size_t      aux_len,
                            size_t      nmemo_insts,
                            size_t      ncounters,
                            size_t      thread_mem_len,
                            size_t      ncaptures)
{
    BruProgram *prog = calloc(1, sizeof(*prog));

    prog->regex = regex;
    stc_vec_init(prog->insts, insts_len);
    stc_vec_init(prog->aux, aux_len);
    prog->nmemo_insts = nmemo_insts;
    stc_vec_init(prog->counters, ncounters);
    prog->thread_mem_len = thread_mem_len;
    prog->ncaptures      = ncaptures;

    memset(prog->insts, 0, insts_len * sizeof(bru_byte_t));
    memset(prog->aux, 0, aux_len * sizeof(bru_byte_t));
    memset(prog->counters, 0, ncounters * sizeof(bru_cntr_t));

    return prog;
}

BruProgram *bru_program_default(const char *regex)
{
    BruProgram *prog = calloc(1, sizeof(*prog));

    prog->regex = regex;
    stc_vec_default_init(prog->insts);
    stc_vec_default_init(prog->aux);
    stc_vec_default_init(prog->counters);

    memset(prog->insts, 0, STC_VEC_DEFAULT_CAP * sizeof(bru_byte_t));
    memset(prog->aux, 0, STC_VEC_DEFAULT_CAP * sizeof(bru_byte_t));
    memset(prog->counters, 0, STC_VEC_DEFAULT_CAP * sizeof(bru_cntr_t));

    return prog;
}

void bru_program_free(BruProgram *self)
{
    free((void *) self->regex);
    stc_vec_free(self->insts);
    stc_vec_free(self->aux);
    stc_vec_free(self->counters);
    free(self);
}

void bru_program_print(const BruProgram *self, FILE *stream)
{
    bru_len_t         i         = 0;
    const bru_byte_t *pc        = self->insts,
                     *insts_end = pc + stc_vec_len_unsafe(self->insts);

    while (pc < insts_end) {
        fprintf(stream, "%4d: ", i++);
        pc = inst_print_formatted(stream, pc, self, print_predicate_as_string,
                                  print_offset_as_absolute_index);
        fputc('\n', stream);
    }
}

void bru_inst_print(FILE *stream, const bru_byte_t *pc)
{
    inst_print_formatted(stream, pc, NULL, print_predicate_as_index,
                         print_offset_as_offset);
}

/* --- Helper function definitions ------------------------------------------ */

static const bru_byte_t *
inst_print_formatted(FILE                  *stream,
                     const bru_byte_t      *pc,
                     const BruProgram      *prog,
                     bru_predicate_print_f *print_predicate,
                     bru_offset_print_f    *print_offset)
{
    char             *p;
    const bru_byte_t *insts = prog ? prog->insts : NULL;
    const bru_byte_t *aux   = prog ? prog->aux : NULL;
    bru_cntr_t        c;
    bru_offset_t      x, y;
    bru_len_t         i, n;

    if (pc == NULL) return NULL;

    switch (BRU_BCREAD(pc)) {
        case BRU_NOOP: fputs("noop", stream); break;
        case BRU_MATCH: fputs("match", stream); break;
        case BRU_BEGIN: fputs("begin", stream); break;
        case BRU_END: fputs("end", stream); break;

        case BRU_MEMO:
            BRU_MEMREAD(n, pc, bru_len_t);
            fprintf(stream, "memo " BRU_LEN_FMT, n);
            break;

        case BRU_CHAR:
            BRU_MEMREAD(p, pc, char *);
            fprintf(stream, "char '%.*s'", stc_utf8_nbytes(p), p);
            break;

        case BRU_PRED:
            BRU_MEMREAD(i, pc, bru_len_t);
            fputs("pred ", stream);
            print_predicate(stream, i, aux);
            break;

        case BRU_SAVE:
            BRU_MEMREAD(n, pc, bru_len_t);
            fprintf(stream, "save " BRU_LEN_FMT, n);
            break;

        case BRU_JMP:
            BRU_MEMREAD(x, pc, bru_offset_t);
            fputs("jmp ", stream);
            print_offset(stream, x, pc, insts);
            break;

        case BRU_SPLIT:
            BRU_MEMREAD(x, pc, bru_offset_t);
            BRU_MEMREAD(y, pc, bru_offset_t);
            fputs("split ", stream);
            print_offset(stream, x, pc - sizeof(bru_offset_t), insts);
            fputs(", ", stream);
            print_offset(stream, y, pc, insts);
            break;

        case BRU_GSPLIT:
            BRU_MEMREAD(x, pc, bru_offset_t);
            fputs("gsplit ", stream);
            print_offset(stream, x, pc, insts);
            break;

        case BRU_LSPLIT:
            BRU_MEMREAD(x, pc, bru_offset_t);
            fputs("lsplit ", stream);
            print_offset(stream, x, pc, insts);
            break;

        case BRU_TSWITCH:
            BRU_MEMREAD(n, pc, bru_len_t);
            fprintf(stream, "tswitch " BRU_LEN_FMT, n);
            for (i = 0; i < n; i++) {
                BRU_MEMREAD(x, pc, bru_offset_t);
                fputs(", ", stream);
                print_offset(stream, x, pc, insts);
            }
            break;

        case BRU_EPSRESET:
            BRU_MEMREAD(n, pc, bru_len_t);
            fprintf(stream, "epsreset " BRU_LEN_FMT, n);
            break;

        case BRU_EPSSET:
            BRU_MEMREAD(n, pc, bru_len_t);
            fprintf(stream, "epsset " BRU_LEN_FMT, n);
            break;

        case BRU_EPSCHK:
            BRU_MEMREAD(n, pc, bru_len_t);
            fprintf(stream, "epschk " BRU_LEN_FMT, n);
            break;

        case BRU_RESET:
            BRU_MEMREAD(i, pc, bru_len_t);
            BRU_MEMREAD(c, pc, bru_cntr_t);
            fprintf(stream, "reset " BRU_LEN_FMT ", " BRU_CNTR_FMT, i, c);
            break;

        case BRU_CMP:
            BRU_MEMREAD(i, pc, bru_len_t);
            BRU_MEMREAD(c, pc, bru_cntr_t);

            switch ((BruOrd) *pc++) {
                case BRU_LT: fputs("cmplt ", stream); break;
                case BRU_LE: fputs("cmple ", stream); break;
                case BRU_EQ: fputs("cmpeq ", stream); break;
                case BRU_NE: fputs("cmpne ", stream); break;
                case BRU_GE: fputs("cmpge ", stream); break;
                case BRU_GT: fputs("cmpgt ", stream); break;
            }

            fprintf(stream, BRU_LEN_FMT ", " BRU_CNTR_FMT, i, c);
            break;

        case BRU_INC:
            BRU_MEMREAD(i, pc, bru_len_t);
            fprintf(stream, "inc " BRU_LEN_FMT, i);
            break;

        case BRU_ZWA:
            BRU_MEMREAD(x, pc, bru_offset_t);
            BRU_MEMREAD(y, pc, bru_offset_t);
            fputs("zwa ", stream);
            print_offset(stream, x, pc - sizeof(bru_offset_t), insts);
            fputs(", ", stream);
            print_offset(stream, y, pc, insts);
            fprintf(stream, ", %d", *pc);
            pc++;
            break;

        case BRU_STATE: fputs("state", stream); break;

        case BRU_WRITE:
            fputs("write", stream);
            fprintf(stream, " 0x%x", *pc);
            pc++;
            break;

        case BRU_WRITE0: fputs("write0", stream); break;

        case BRU_WRITE1: fputs("write1", stream); break;

        case BRU_NBYTECODES:
            fprintf(stderr, "bytecode = %d\n", pc[-1]);
            assert(0 && "unreachable");
    }

    return pc;
}

static void
print_predicate_as_string(FILE *stream, bru_len_t idx, const bru_byte_t *aux)
{
    char *p = bru_intervals_to_str((const BruIntervals *) (aux + idx));
    fputs(p, stream);
    free(p);
}

static void
print_predicate_as_index(FILE *stream, bru_len_t idx, const bru_byte_t *aux)
{
    BRU_UNUSED(aux);
    fprintf(stream, BRU_LEN_FMT, idx);
}

static void print_offset_as_absolute_index(FILE             *stream,
                                           bru_offset_t      x,
                                           const bru_byte_t *pc,
                                           const bru_byte_t *insts)
{
    bru_len_t idx, len;

    pc += x;
    for (idx = 0; insts < pc; idx++) {
        switch (BRU_BCREAD(insts)) {
            case BRU_NOOP:  /* fallthrough */
            case BRU_MATCH: /* fallthrough */
            case BRU_BEGIN: /* fallthrough */
            case BRU_END: break;
            case BRU_MEMO: insts += sizeof(bru_len_t); break;
            case BRU_CHAR: insts += sizeof(char *); break;
            case BRU_PRED: /* fallthrough */
            case BRU_SAVE: insts += sizeof(bru_len_t); break;
            case BRU_JMP:    /* fallthrough */
            case BRU_GSPLIT: /* fallthrough */
            case BRU_LSPLIT: insts += sizeof(bru_offset_t); break;
            case BRU_SPLIT: insts += 2 * sizeof(bru_offset_t); break;
            case BRU_TSWITCH:
                BRU_MEMREAD(len, insts, bru_len_t);
                insts += len * sizeof(bru_offset_t);
                break;
            case BRU_EPSRESET: /* fallthrough */
            case BRU_EPSSET:   /* fallthrough */
            case BRU_EPSCHK: insts += sizeof(bru_len_t); break;
            case BRU_RESET:
                insts += sizeof(bru_len_t) + sizeof(bru_cntr_t);
                break;
            case BRU_CMP:
                insts += sizeof(bru_len_t) + sizeof(bru_cntr_t) + 1;
                break;
            case BRU_INC: insts += sizeof(bru_len_t); break;
            case BRU_ZWA: insts += 2 * sizeof(bru_offset_t) + 1; break;
            case BRU_STATE: break;
            case BRU_WRITE: insts += sizeof(bru_byte_t); break;
            case BRU_WRITE0: break;
            case BRU_WRITE1: break;
            case BRU_NBYTECODES:
                fprintf(stderr, "bytecode = %d\n", insts[-1]);
                assert(0 && "unreachable");
        }
    }

    fprintf(stream, BRU_LEN_FMT, idx);
}

static void print_offset_as_offset(FILE             *stream,
                                   bru_offset_t      offset,
                                   const bru_byte_t *pc,
                                   const bru_byte_t *insts)
{
    BRU_UNUSED(pc);
    BRU_UNUSED(insts);
    fprintf(stream, BRU_OFFSET_FMT, offset);
}
