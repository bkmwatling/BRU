#include <stdlib.h>

#include "compiler.h"
#include "glushkov.h"
#include "sre.h"
#include "thompson.h"

#define COMPILER_OPTS_DEFAULT ((CompilerOpts){ THOMPSON, FALSE, SC_SPLIT })

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
    Regex         *re;
    const Program *prog = NULL;

    re = parser_parse(self->parser);
    if (self->opts.construction == THOMPSON) {
        prog = thompson_compile(re);
    } else if (self->opts.construction == GLUSHKOV) {
        prog = glushkov_compile(re);
    }
    regex_free(re);

    return prog;
}
