#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

size_t count(Regex *re, size_t *aux_size)
{
    size_t n = 0;

    switch (re->type) {
        case CARET:
        case DOLLAR: n = sizeof(char); break;

        case LITERAL:
            n          = sizeof(char) + sizeof(size_t) + sizeof(Interval *);
            *aux_size += sizeof(Interval);
            break;
        case CC:
            n          = sizeof(char) + sizeof(size_t) + sizeof(Interval *);
            *aux_size += re->cc_len * sizeof(Interval);
            break;
        case ALT:
            n  = 2 * sizeof(char) + 3 * sizeof(Inst *);
            n += count(re->left, aux_size) + count(re->right, aux_size);
            break;
        case CONCAT:
            n = count(re->left, aux_size) + count(re->right, aux_size);
            break;
        case CAPTURE:
            n  = 2 * sizeof(char) + 2 * sizeof(char **);
            n += count(re->left, aux_size);
            break;
        case STAR:
            n  = 5 * sizeof(char) + 5 * sizeof(Inst *) + 2 * sizeof(size_t *);
            n += count(re->left, aux_size);
            break;
        case PLUS:
            n  = 4 * sizeof(char) + 3 * sizeof(Inst *) + 2 * sizeof(size_t *);
            n += count(re->left, aux_size);
            break;
        case QUES:
            n  = sizeof(char) + 2 * sizeof(Inst *);
            n += count(re->left, aux_size);
            break;
        case COUNTER:
            n = 9 * sizeof(char) + 5 * sizeof(Inst *) + 6 * sizeof(size_t *) +
                3 * sizeof(cntr_t);
            n += count(re->left, aux_size);
            break;
        case LOOKAHEAD:
            n  = 2 * sizeof(char) + 2 * sizeof(Inst *) + sizeof(int);
            n += count(re->left, aux_size);
            break;
    }

    return n;
}

Inst *emit(Regex *re, Inst *pc) { return pc; }

Program *compile(Compiler *compiler) { return NULL; }

void compiler_free(Compiler *compiler) { free(compiler->parser); }
