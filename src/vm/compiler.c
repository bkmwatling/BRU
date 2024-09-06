#include <stdlib.h>

#include "../fa/constructions/glushkov.h"
#include "../fa/constructions/thompson.h"
#include "../fa/transformers/flatten.h"
#include "../fa/transformers/memoisation.h"
#include "../re/sre.h"
#include "../utils.h"
#include "compiler.h"

#define COMPILER_OPTS_DEFAULT \
    ((BruCompilerOpts){ BRU_THOMPSON, FALSE, BRU_CS_PCRE, BRU_MS_NONE, FALSE })

#define SET_OFFSET(p, pc) (*(p) = pc - (byte *) ((p) + 1))

static void compile_state_markers(void *meta, BruProgram *prog)
{
    BRU_UNUSED(meta);
    BRU_BCPUSH(prog->insts, BRU_STATE);
}

BruCompiler *bru_compiler_new(const BruParser      *parser,
                              const BruCompilerOpts opts)
{
    BruCompiler *compiler = malloc(sizeof(*compiler));

    compiler->parser = parser;
    compiler->opts   = opts;

    return compiler;
}

BruCompiler *bru_compiler_default(const BruParser *parser)
{
    return bru_compiler_new(parser, COMPILER_OPTS_DEFAULT);
}

void bru_compiler_free(BruCompiler *self)
{
    bru_parser_free((BruParser *) self->parser);
    free(self);
}

const BruProgram *bru_compiler_compile(const BruCompiler *self)
{
    BruRegex          re;
    BruParseResult    res;
    const BruProgram *prog;
    BruStateMachine  *sm = NULL, *tmp = NULL;

    res = bru_parser_parse(self->parser, &re);
    if (res.code != BRU_PARSE_SUCCESS) return NULL;

    switch (self->opts.construction) {
        case BRU_THOMPSON: sm = bru_thompson_construct(re, &self->opts); break;
        case BRU_GLUSHKOV: sm = bru_glushkov_construct(re, &self->opts); break;
        case BRU_FLAT:
            tmp = bru_thompson_construct(re, &self->opts);
            sm  = bru_transform_flatten(tmp, self->parser->opts.logfile);
            bru_smir_free(tmp);
            break;
    }
    bru_regex_node_free(re.root);

    if (self->opts.memo_scheme != BRU_MS_NONE)
        sm = bru_transform_memoise(sm, self->opts.memo_scheme,
                                   self->parser->opts.logfile);

#ifdef BRU_DEBUG
    bru_smir_print(sm, stderr);
#endif /* BRU_DEBUG */

    prog = bru_smir_compile_with_meta(
        sm, self->opts.mark_states ? compile_state_markers : NULL, NULL);
    bru_smir_free(sm);

    return prog;
}
