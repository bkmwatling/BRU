#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

size_t count(Regex *re, size_t *aux_size)
{
    size_t ninst = 0;

    switch (re->type) {
        case CARET: ninst = 1; break;
        case DOLLAR: ninst = 1; break;
        case LITERAL: ninst = 1; break;
        case CC: ninst = 1; break;
        case ALT:
            ninst = 1 + count(re->left, aux_size) + count(re->right, aux_size);
            break;
        case CONCAT:
            ninst = count(re->left, aux_size) + count(re->right, aux_size);
            break;
        case CAPTURE: ninst = 2 + count(re->left, aux_size); break;
        case STAR: ninst = 1 + count(re->left, aux_size); break;
        case PLUS: ninst = 1 + count(re->left, aux_size); break;
        case QUES: ninst = 1 + count(re->left, aux_size); break;
        case COUNTER: ninst = 1 + count(re->left, aux_size); break;
        case LOOKAHEAD:
            ninst = 1 + count(re->left, aux_size) + count(re->right, aux_size);
            break;
    }

    return ninst;
}

Inst *emit(Regex *re, Inst *pc) { return pc; }

Program *compile(Compiler *compiler) { return NULL; }

void compiler_free(Compiler *compiler) { free(compiler->parser); }
