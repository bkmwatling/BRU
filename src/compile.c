#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "parser.h"
#include "srvm.h"
#include "stc/util/args.h"

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

static StcArgConvertResult convert_construction(const char *arg, void *out)
{
    Construction *construction = out;

    if (strcmp(arg, "thompson") == 0) {
        *construction = THOMPSON;
    } else if (strcmp(arg, "glushkov") == 0) {
        *construction = GLUSHKOV;
    } else {
        /* fprintf(stderr, "ERROR: invalid construction\n"); */
        return STC_CR_FAILURE;
    }

    return STC_CR_SUCCESS;
}

static StcArgConvertResult convert_branch(const char *arg, void *out)
{
    SplitChoice *branch = out;

    if (strcmp(arg, "split") == 0) {
        *branch = SC_SPLIT;
    } else if (strcmp(arg, "tswitch") == 0) {
        *branch = SC_TSWITCH;
    } else if (strcmp(arg, "lswitch") == 0) {
        *branch = SC_LSWITCH;
    } else {
        /* fprintf(stderr, "ERROR: invalid branching type\n"); */
        return STC_CR_FAILURE;
    }

    return STC_CR_SUCCESS;
}

int main(int argc, const char **argv)
{
    char        *regex, *prog_str;
    Compiler    *c;
    Program     *prog;
    CompilerOpts compiler_opts;
    ParserOpts   parser_opts;
    StcArg       args[] = {
        { STC_ARG_BOOL, "-o", "--only-counters", &parser_opts.only_counters,
                NULL,
                "whether to use just counters and treat *, +, and ? as counters",
                NULL, NULL },
        { STC_ARG_BOOL, "-u", "--unbounded-counters",
                &parser_opts.unbounded_counters, NULL,
                "whether to permit unbounded counters or substitute with *", NULL,
                NULL },
        { STC_ARG_BOOL, "-w", "--whole-match-capture",
                &parser_opts.whole_match_capture, NULL,
                "whether to have the whole regex match be the 0th capture", NULL,
                NULL },
        { STC_ARG_CUSTOM, "-c", "--construction", &compiler_opts.construction,
                "thompson | glushkov",
                "whether to compile using thompson or glushkov", "thompson",
                convert_construction },
        { STC_ARG_BOOL, "-s", "--only-std-split", &compiler_opts.only_std_split,
                NULL, "whether to use of standard `split` instruction only", NULL,
                NULL },
        { STC_ARG_CUSTOM, "-b", "--branch", &compiler_opts.branch,
                "split | tswitch | lswitch",
                "which type of branching to use for control flow", "split",
                convert_branch },
        { STC_ARG_STR, "<regex>", NULL, &prog_str, NULL,
                "the regex to compile to SRVM instructions", NULL, NULL }
    };

    stc_args_parse_exact(argc, argv, args, ARR_LEN(args), NULL);

    regex = malloc((strlen(prog_str) + 1) * sizeof(char));
    strcpy(regex, prog_str);
    c        = compiler_new(parser_new(regex, &parser_opts), &compiler_opts);
    prog     = compile(c);
    prog_str = program_to_str(prog);

    printf("%s\n", prog_str);

    compiler_free(c);
    program_free(prog);
    free(prog_str);

    return EXIT_SUCCESS;
}
