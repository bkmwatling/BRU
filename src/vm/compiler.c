#include <assert.h>
#include <stdlib.h>

#include <bru/utils.h>
#include <bru/vm/compiler.h>
#include <bru/vm/optimise.h>
#include <bru/vm/program.h>

/* --- Helper function definitions ------------------------------------------ */

static size_t instruction_size(BruInstruction instruction)
{
    size_t size = sizeof(bru_byte_t);

    switch (instruction.bytecode) {
        case BRU_NOOP: size = 0; break;

        case BRU_MATCH: /* fallthrough */
        case BRU_BEGIN: /* fallthrough */
        case BRU_END: break;

        case BRU_CHAR: size += sizeof(instruction.ch); break;
        case BRU_PRED: size += sizeof(bru_len_t); break;

        case BRU_GSPLIT: /* fallthrough */
        case BRU_LSPLIT: /* fallthrough */
        case BRU_JMP: size += sizeof(bru_offset_t); break;

        case BRU_SPLIT: size += 2 * sizeof(bru_offset_t); break;
        case BRU_TSWITCH:
            size += sizeof(bru_len_t) +
                    stc_vec_len(instruction.tswitch) * sizeof(bru_offset_t);
            break;

        case BRU_SAVE: size += sizeof(instruction.idx); break;
        case BRU_BACKREF: size += sizeof(instruction.idx); break;

        case BRU_INC: size += sizeof(instruction.idx); break;
        case BRU_SET:
            size += sizeof(instruction.idx) + sizeof(instruction.val);
            break;
        case BRU_CMP:
            size += sizeof(instruction.idx) + sizeof(instruction.val) +
                    sizeof(bru_byte_t);
            break;

        case BRU_EPSRESET: /* fallthrough */
        case BRU_EPSSET:   /* fallthrough */
        case BRU_EPSCHK: size += sizeof(instruction.idx); break;

        case BRU_MEMOSET: /* fallthrough */
        case BRU_MEMOCHK: size += sizeof(instruction.idx); break;

        case BRU_ZWA: assert(FALSE && "TOD: ZWA compilationO");

        case BRU_STATE: break;

        case BRU_WRITE: size += sizeof(instruction.c); break;
        case BRU_WRITE0: /* fallthrough */
        case BRU_WRITE1: break;

        case BRU_NBYTECODES: assert(FALSE && "unreachable"); break;
    }

    return size;
}

static void populate_memory_requirements(StcVec(BruInstruction) instructions,
                                         BruProgram            *prog)
{
    size_t          i, n = stc_vec_len(instructions);
    size_t          program_size;
    BruInstruction *instr;

    for (i = 0, program_size = 0; i < n; i++) {
        instr               = &instructions[i];
        instr->prog_offset  = program_size;
        program_size       += instruction_size(*instr);

        /**
         * collect meta-information about the program, such as total memory
         * required for captures, counters, memoisation, etc.
         */
        switch (instr->bytecode) {

            case BRU_PRED: break;

            case BRU_SAVE:
                if (2 * prog->ncaptures <= instr->idx)
                    prog->ncaptures = (instr->idx / 2) + 1;
                break;

            case BRU_BACKREF: prog->requires_backref = TRUE; break;

            case BRU_INC: /* fallthrough */
            case BRU_SET: /* fallthrough */
            case BRU_CMP:
                if (prog->ncounters <= instr->idx)
                    prog->ncounters = instr->idx + 1;
                break;

            case BRU_EPSRESET: /* fallthrough */
            case BRU_EPSSET:   /* fallthrough */
            case BRU_EPSCHK:
                if (sizeof(const char *) * (instr->idx + 1) >
                    prog->thread_mem_len) {
                    prog->thread_mem_len =
                        sizeof(const char *) * (instr->idx + 1);
                }
                break;

            case BRU_MEMOSET: /* fallthrough */
            case BRU_MEMOCHK:
                if (prog->nmemo_insts <= instr->idx)
                    prog->nmemo_insts = instr->idx + 1;
                break;

            case BRU_WRITE:  /* fallthrough */
            case BRU_WRITE0: /* fallthrough */
            case BRU_WRITE1: prog->requires_writing = TRUE; break;

            case BRU_NOOP:    /* fallthrough */
            case BRU_MATCH:   /* fallthrough */
            case BRU_BEGIN:   /* fallthrough */
            case BRU_END:     /* fallthrough */
            case BRU_CHAR:    /* fallthrough */
            case BRU_JMP:     /* fallthrough */
            case BRU_SPLIT:   /* fallthrough */
            case BRU_GSPLIT:  /* fallthrough */
            case BRU_LSPLIT:  /* fallthrough */
            case BRU_TSWITCH: /* fallthrough */
            case BRU_STATE: break;

            case BRU_ZWA: assert(FALSE && "TODO: ZWA compilation"); break;
            case BRU_NBYTECODES: assert(FALSE && "UNREACHABLE"); break;
        }
    }

    stc_vec_reserve(&prog->insts, program_size);
    stc_vec_len(prog->insts) = program_size * sizeof(*prog->insts);
}

static bru_offset_t compute_offset(size_t jmp_start, size_t jmp_end)
{
    if (jmp_end > jmp_start)
        return (bru_offset_t) (jmp_end - jmp_start);
    else
        return -(bru_offset_t) (jmp_start - jmp_end);
}

static bru_byte_t *compile_instruction(bru_byte_t    *pc,
                                       BruProgram    *prog,
                                       BruInstruction instruction)
{
    size_t jmp_point, n, i;

    *pc++ = instruction.bytecode;

    switch (instruction.bytecode) {
        case BRU_NOOP: pc--; break;

        case BRU_MATCH: /* fallthrough */
        case BRU_BEGIN: /* fallthrough */
        case BRU_END: break;

        case BRU_CHAR: BRU_MEMWRITE(pc, const char *, instruction.ch); break;
        case BRU_PRED:
            BRU_MEMWRITE(pc, bru_len_t, stc_vec_len(prog->aux));
            BRU_MEMCPY(&prog->aux, instruction.pred,
                       sizeof(*instruction.pred) +
                           instruction.pred->len *
                               sizeof(*instruction.pred->intervals));
            break;

        case BRU_JMP:
            jmp_point = pc - prog->insts + sizeof(bru_offset_t);
            BRU_MEMWRITE(
                pc, bru_offset_t,
                compute_offset(jmp_point, instruction.jmp->prog_offset));
            break;
        case BRU_SPLIT:
            jmp_point = pc - prog->insts + sizeof(bru_offset_t);
            BRU_MEMWRITE(
                pc, bru_offset_t,
                compute_offset(jmp_point, instruction.split_left->prog_offset));
            jmp_point += sizeof(bru_offset_t);
            BRU_MEMWRITE(pc, bru_offset_t,
                         compute_offset(jmp_point,
                                        instruction.split_right->prog_offset));
            break;
        case BRU_GSPLIT:
            jmp_point = pc - prog->insts + sizeof(bru_offset_t);
            BRU_MEMWRITE(
                pc, bru_offset_t,
                compute_offset(jmp_point, instruction.jmp->prog_offset));
            break;
        case BRU_LSPLIT:
            jmp_point = pc - prog->insts + sizeof(bru_offset_t);
            BRU_MEMWRITE(
                pc, bru_offset_t,
                compute_offset(jmp_point, instruction.jmp->prog_offset));
            break;
        case BRU_TSWITCH:
            n = stc_vec_len(instruction.tswitch);
            BRU_MEMWRITE(pc, bru_len_t, n);
            jmp_point = pc - prog->insts;
            for (i = 0; i < n; i++) {
                jmp_point += sizeof(bru_offset_t);
                BRU_MEMWRITE(
                    pc, bru_offset_t,
                    compute_offset(jmp_point,
                                   instruction.tswitch[i]->prog_offset));
            }
            break;

        case BRU_SAVE:
            BRU_MEMWRITE(pc, bru_len_t, instruction.idx);
            if ((instruction.idx / 2) + 1 > prog->ncaptures)
                prog->ncaptures = (instruction.idx / 2) + 1;
            break;

        case BRU_BACKREF: BRU_MEMWRITE(pc, bru_len_t, instruction.idx); break;

        case BRU_INC: BRU_MEMWRITE(pc, bru_len_t, instruction.idx); break;
        case BRU_SET:
            BRU_MEMWRITE(pc, bru_len_t, instruction.idx);
            BRU_MEMWRITE(pc, bru_cntr_t, instruction.val);
            break;
        case BRU_CMP:
            BRU_MEMWRITE(pc, bru_len_t, instruction.idx);
            BRU_MEMWRITE(pc, bru_cntr_t, instruction.val);
            BRU_MEMWRITE(pc, bru_byte_t, instruction.ord);
            break;

        case BRU_EPSRESET: /* fallthrough */
        case BRU_EPSSET:   /* fallthrough */
        case BRU_EPSCHK: BRU_MEMWRITE(pc, bru_len_t, instruction.idx); break;

        case BRU_MEMOSET: /* fallthrough */
        case BRU_MEMOCHK: BRU_MEMWRITE(pc, bru_len_t, instruction.idx); break;

        case BRU_ZWA: assert(FALSE && "TODO: ZWA compilation");

        case BRU_STATE: break;

        case BRU_WRITE:
            prog->requires_writing = TRUE;
            BRU_MEMWRITE(pc, char, instruction.c);
            break;
        case BRU_WRITE0: /* fallthrough */
        case BRU_WRITE1: prog->requires_writing = TRUE; break;

        case BRU_NBYTECODES: assert(FALSE && "unreachable"); break;
    }

    return pc;
}

/* --- API function definitions --------------------------------------------- */

BruProgram *bru_compiler_compile(const char            *regex,
                                 StcVec(BruInstruction) instructions,
                                 BruOptimisationLevel   op_level)
{
    BRU_UNUSED(op_level);
    BruProgram *prog = bru_program_default(regex);
    bru_byte_t *pc;
    size_t      i, n = stc_vec_len(instructions);

    // optimise
    switch (op_level) {
        case BRU_OPTIMISE_NONE: break;
        case BRU_OPTIMISE_FULL:
            bru_optimise_compress_control_flow_chain(instructions);
            bru_optimise_remove_unnecessary_jumps(instructions);
            bru_optimise_remove_dead_code(instructions);
            break;
    }

    // compile instruction
    populate_memory_requirements(instructions, prog);
    for (pc = prog->insts, i = 0; i < n; i++)
        pc = compile_instruction(pc, prog, instructions[i]);

    return prog;
}
