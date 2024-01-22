#include <stdlib.h>

#include "compiler.h"
#include "glushkov.h"
#include "sre.h"
#include "thompson.h"
#include "walkers/memoisation.h"

#ifdef DEBUG
#    include "walkers/regex_to_string.h"
#    include <stdio.h>
#endif

#define COMPILER_OPTS_DEFAULT \
    ((CompilerOpts){ THOMPSON, FALSE, SC_SPLIT, CS_PCRE, MS_NONE })

#define SET_OFFSET(p, pc) (*(p) = pc - (byte *) ((p) + 1))

Compiler *compiler_new(const Parser *parser, const CompilerOpts *opts)
{
    Compiler *compiler = malloc(sizeof(Compiler));
    compiler->parser   = parser;
    compiler->opts     = opts ? *opts : COMPILER_OPTS_DEFAULT;
    return compiler;
}

void compiler_free(Compiler *self)
{
    parser_free((Parser *) self->parser);
    free(self);
}

const Program *compiler_compile(const Compiler *self)
{
    Regex          re;
    StateMachine  *sm = NULL;
    const Program *prog;

    re = parser_parse(self->parser);
    switch (self->opts.construction) {
        case THOMPSON: sm = thompson_construct(re, &self->opts); break;
        case GLUSHKOV: sm = glushkov_construct(re, &self->opts); break;
    }
    regex_node_free(re.root);
    prog = smir_compile(sm);
    smir_free(sm);

    return prog;
}
