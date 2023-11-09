#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STC_SV_ENABLE_SHORT_NAMES
#include "stc/fatp/string_view.h"
#define STC_ARGS_ENABLE_SHORT_NAMES
#include "stc/util/args.h"
#define STC_UTF_ENABLE_SHORT_NAMES
#include "stc/util/utf.h"

#include "compiler.h"
#include "lockstep.h"
#include "parser.h"
#include "scheduler.h"
#include "spencer.h"
#include "srvm.h"

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef enum { CMD_PARSE, CMD_COMPILE, CMD_MATCH } Subcommand;

typedef enum { SCH_SPENSER, SCH_LOCKSTEP } SchedulerType;

static ArgConvertResult convert_subcommand(const char *arg, void *out)
{
    Subcommand *cmd = out;

    if (strcmp(arg, "parse") == 0) {
        *cmd = CMD_PARSE;
    } else if (strcmp(arg, "compile") == 0) {
        *cmd = CMD_COMPILE;
    } else if (strcmp(arg, "match") == 0) {
        *cmd = CMD_MATCH;
    } else {
        return STC_CR_FAILURE;
    }

    return STC_CR_SUCCESS;
}

static ArgConvertResult convert_scheduler_type(const char *arg, void *out)
{
    SchedulerType *type = out;

    if (strcmp(arg, "spencer") == 0) {
        *type = SCH_SPENSER;
    } else if (strcmp(arg, "lockstep") == 0 || strcmp(arg, "thompson") == 0) {
        *type = SCH_LOCKSTEP;
    } else {
        return STC_CR_FAILURE;
    }

    return STC_CR_SUCCESS;
}

static ArgConvertResult convert_construction(const char *arg, void *out)
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

static ArgConvertResult convert_branch(const char *arg, void *out)
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
    int            arg_idx;
    char          *regex, *text, *s;
    Subcommand     cmd;
    SchedulerType  scheduler_type;
    Parser        *p;
    Compiler      *c;
    Regex         *re;
    Program       *prog;
    CompilerOpts   compiler_opts;
    ParserOpts     parser_opts;
    ThreadManager *thread_manager = NULL;
    Scheduler     *scheduler      = NULL;
    SRVM          *srvm;
    StringView     capture, *captures;
    len_t          i, ncaptures;
    size_t         ncodepoints;
    Arg            args[] = {
        { STC_ARG_CUSTOM, "<subcommand>", NULL, &cmd, NULL,
                     "the subcommand to run (parse, compile, or match)", NULL,
                     convert_subcommand },
        { STC_ARG_STR, "<regex>", NULL, &regex, NULL,
                     "the regex to compile to SRVM instructions", NULL, NULL },
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
        { STC_ARG_BOOL, NULL, "--only-std-split", &compiler_opts.only_std_split,
                     NULL, "whether to use of standard `split` instruction only", NULL,
                     NULL },
        { STC_ARG_CUSTOM, "-b", "--branch", &compiler_opts.branch,
                     "split | tswitch | lswitch",
                     "which type of branching to use for control flow", "split",
                     convert_branch },
        { STC_ARG_CUSTOM, "-s", "--scheduler", &scheduler_type,
                     "spencer | lockstep | thompson",
                     "which scheduler to use for execution", "spencer",
                     convert_scheduler_type },
        { STC_ARG_STR, "<input>", NULL, &text, NULL,
                     "the input string to match against the regex", NULL, NULL }
    };

    arg_idx  = args_parse(argc, argv, args, 1, NULL);
    argc    -= arg_idx - 1;
    argv    += arg_idx - 1;

    if (cmd == CMD_PARSE) {
        args_parse_exact(argc, argv, args + 1, 4, NULL);

        p  = parser_new(regex, &parser_opts);
        re = parser_parse(p);
        s  = regex_to_tree_str(re);

        printf("%s\n", s);

        parser_free(p);
        regex_free(re);
        free(s);
    } else if (cmd == CMD_COMPILE) {
        args_parse_exact(argc, argv, args + 1, ARR_LEN(args) - 3, NULL);

        c    = compiler_new(parser_new(regex, &parser_opts), &compiler_opts);
        prog = compiler_compile(c);
        s    = program_to_str(prog);

        printf("%s\n", s);

        compiler_free(c);
        program_free(prog);
        free(s);
    } else if (cmd == CMD_MATCH) {
        args_parse_exact(argc, argv, args + 1, ARR_LEN(args) - 1, NULL);

        c    = compiler_new(parser_new(regex, &parser_opts), &compiler_opts);
        prog = compiler_compile(c);
        if (scheduler_type == SCH_SPENSER) {
            thread_manager = spencer_thread_manager_new();
            scheduler =
                spencer_scheduler_to_scheduler(spencer_scheduler_new(prog));
        } else if (scheduler_type == SCH_LOCKSTEP) {
            thread_manager = thompson_thread_manager_new();
            scheduler      = thompson_scheduler_to_scheduler(
                thompson_scheduler_new(prog, text));
        }

        srvm = srvm_new(thread_manager, scheduler);
        printf("matched = %d\n", srvm_match(srvm, text));
        printf("captures:\n");
        captures = srvm_captures(srvm, &ncaptures);
        printf("  input: '%s'\n", text);
        for (i = 0; i < ncaptures; i++) {
            capture = captures[i];
            printf("%7hu: ", i);
            if (capture.str) {
                ncodepoints = utf8_str_ncodepoints(text) -
                              utf8_str_ncodepoints(capture.str);
                printf("%*s'" SV_FMT "'\n", (int) ncodepoints, "",
                       SV_ARG(capture));
            } else {
                printf("not captured\n");
            }
        }

        compiler_free(c);
        program_free(prog);
        srvm_free(srvm);
        free(captures);
    }

    return EXIT_SUCCESS;
}
