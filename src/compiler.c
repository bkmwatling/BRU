#include <stdlib.h>

#include "compiler.h"
#include "glushkov.h"
#include "walkers/memoisation.h"
#include "sre.h"
#include "thompson.h"

#ifdef DEBUG
#include <stdio.h>
#include "walkers/regex_to_string.h"
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

static void memoise(Regex **re, Construction c, MemoScheme ms)
{
    memoise_f memo;

    switch (ms) {
        case MS_NONE: return;
        case MS_CN: memo = closure_node_thompson; break;
        case MS_IN:
            switch (c) {
                case THOMPSON: memo = in_degree_thompson; break;
                case GLUSHKOV: memo = in_degree_glushkov; break;
            }
            break;
        case MS_IAR: memo = infinite_ambiguity_removal_thompson; break;
    }

    memo(re);
}

const Program *compiler_compile(const Compiler *self)
{
    Regex         *re;
    const Program *prog = NULL;
#ifdef DEBUG
    char *re_str;
#endif

    re = parser_parse(self->parser);
    memoise(&re, self->opts.construction, self->opts.memo_scheme);

#ifdef DEBUG
    re_str = regex_to_string(re);
    printf("compiling regex: %s\n", re_str);
    free(re_str);
#endif

    switch (self->opts.construction) {
        case THOMPSON: prog = thompson_compile(re, &self->opts); break;
        case GLUSHKOV: prog = glushkov_compile(re, &self->opts); break;
    }
    regex_free(re);

    return prog;
}
