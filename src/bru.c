#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stc/fatp/string_view.h"
#include "stc/util/args.h"
#include "stc/util/utf.h"

#include "re/parser.h"
#include "vm/compiler.h"
#include "vm/srvm.h"
#include "vm/thread_managers/all_matches.h"
#include "vm/thread_managers/benchmark.h"
#include "vm/thread_managers/lockstep.h"
#include "vm/thread_managers/memoisation.h"
#include "vm/thread_managers/spencer.h"

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef enum { CMD_PARSE, CMD_COMPILE, CMD_MATCH } Subcommand;

typedef enum { SCH_SPENCER, SCH_LOCKSTEP } SchedulerType;

static char *sdup(const char *s)
{
    char  *str;
    size_t len = strlen(s) + 1;

    str = malloc(len * sizeof(char));
    memcpy(str, s, len * sizeof(char));

    return str;
}

static StcArgConvertResult convert_subcommand(const char *arg, void *out)
{
    Subcommand *cmd = out;

    if (strcmp(arg, "parse") == 0)
        *cmd = CMD_PARSE;
    else if (strcmp(arg, "compile") == 0)
        *cmd = CMD_COMPILE;
    else if (strcmp(arg, "match") == 0)
        *cmd = CMD_MATCH;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_filepath(const char *arg, void *out)
{
    FILE **logfile = out;

    if (strcmp(arg, "stdout") == 0)
        *logfile = stdout;
    else if (strcmp(arg, "stderr") == 0)
        *logfile = stderr;
    else if ((*logfile = fopen(arg, "a")) == NULL)
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_construction(const char *arg, void *out)
{
    Construction *construction = out;

    if (strcmp(arg, "thompson") == 0)
        *construction = THOMPSON;
    else if (strcmp(arg, "glushkov") == 0)
        *construction = GLUSHKOV;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_branch(const char *arg, void *out)
{
    SplitChoice *branch = out;

    if (strcmp(arg, "split") == 0)
        *branch = SC_SPLIT;
    else if (strcmp(arg, "tswitch") == 0)
        *branch = SC_TSWITCH;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_capture_semantics(const char *arg, void *out)
{
    CaptureSemantics *cs = out;

    if (strcmp(arg, "pcre") == 0)
        *cs = CS_PCRE;
    else if (strcmp(arg, "re2") == 0)
        *cs = CS_RE2;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_memo_scheme(const char *arg, void *out)
{
    MemoScheme *ms = out;

    if (strcmp(arg, "in") == 0 || strcmp(arg, "IN") == 0)
        *ms = MS_IN;
    else if (strcmp(arg, "cn") == 0 || strcmp(arg, "CN") == 0)
        *ms = MS_CN;
    else if (strcmp(arg, "iar") == 0 || strcmp(arg, "IAR") == 0)
        *ms = MS_IAR;
    else if (strcmp(arg, "none") == 0)
        *ms = MS_NONE;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_scheduler_type(const char *arg, void *out)
{
    SchedulerType *type = out;

    if (strcmp(arg, "spencer") == 0)
        *type = SCH_SPENCER;
    else if (strcmp(arg, "lockstep") == 0 || strcmp(arg, "thompson") == 0)
        *type = SCH_LOCKSTEP;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

int main(int argc, const char **argv)
{
    int            arg_idx, exit_code = EXIT_SUCCESS;
    int            benchmark, matched, all_matches;
    char          *regex, *text;
    FILE          *outfile, *logfile;
    Subcommand     cmd;
    SchedulerType  scheduler_type;
    Parser        *p;
    Compiler      *c;
    Regex          re;
    ParseResult    res;
    const Program *prog;
    CompilerOpts   compiler_opts;
    ParserOpts     parser_opts;
    ThreadManager *thread_manager = NULL;
    SRVM          *srvm;
    StcStringView  capture, *captures;
    len_t          i, ncaptures;
    size_t         ncodepoints;
    StcArg         args[] = {
        { STC_ARG_CUSTOM, "<subcommand>", NULL, &cmd, NULL,
                  "the subcommand to run (parse, compile, or match)", NULL,
                  convert_subcommand },
        { STC_ARG_STR, "<regex>", NULL, &regex, NULL, "the regex to work with",
                  NULL, NULL },
        { STC_ARG_CUSTOM, "-o", "--outfile", &outfile,
                  "filepath | stdout | stderr", "the file for normal output", "stdout",
                  convert_filepath },
        { STC_ARG_CUSTOM, "-l", "--logfile", &logfile,
                  "filepath | stdout | stderr", "the file for logging output", "stderr",
                  convert_filepath },
        { STC_ARG_BOOL, NULL, "--only-counters", &parser_opts.only_counters,
                  NULL,
                  "whether to use just counters and treat *, +, and ? as counters",
                  NULL, NULL },
        { STC_ARG_BOOL, "-u", "--unbounded-counters",
                  &parser_opts.unbounded_counters, NULL,
                  "whether to permit unbounded counters or substitute with *", NULL,
                  NULL },
        { STC_ARG_BOOL, "-e", "--expand-counters", &parser_opts.expand_counters,
                  NULL, "whether to expand counters with concatenation and nested ?",
                  NULL, NULL },
        { STC_ARG_BOOL, "-w", "--whole-match-capture",
                  &parser_opts.whole_match_capture, NULL,
                  "whether to have the whole regex match be the 0th capture", NULL,
                  NULL },
        { STC_ARG_CUSTOM, "-c", "--construction", &compiler_opts.construction,
                  "thompson | glushkov",
                  "whether to compile using thompson or glushkov", "thompson",
                  convert_construction },
        { STC_ARG_BOOL, NULL, "--only-std-split", &compiler_opts.only_std_split,
                  NULL, "whether to use standard `split` instruction only", NULL,
                  NULL },
        { STC_ARG_CUSTOM, "-b", "--branch", &compiler_opts.branch,
                  "split | tswitch", "which type of branching to use for control flow",
                  "split", convert_branch },
        { STC_ARG_CUSTOM, NULL, "--capture-semantics",
                  &compiler_opts.capture_semantics, "pcre | re2",
                  "which type of capturing semantics to compile with", "pcre",
                  convert_capture_semantics },
        { STC_ARG_CUSTOM, "-m", "--memo-scheme", &compiler_opts.memo_scheme,
                  "none | cn | in | iar", "which memoisation scheme to apply", "none",
                  convert_memo_scheme },
        { STC_ARG_CUSTOM, "-s", "--scheduler", &scheduler_type,
                  "spencer | lockstep | thompson",
                  "which scheduler to use for execution", "spencer",
                  convert_scheduler_type },
        { STC_ARG_BOOL, NULL, "--benchmark", &benchmark, NULL,
                  "whether to benchmark SRVM execution", NULL, NULL },
        { STC_ARG_BOOL, NULL, "--all-matches", &all_matches, NULL,
                  "whether to report all matches", NULL, NULL },
        { STC_ARG_STR, "<input>", NULL, &text, NULL,
                  "the input string to match against the regex", NULL, NULL }
    };

    arg_idx  = stc_args_parse(argc, argv, args, 1, NULL);
    argc    -= arg_idx - 1;
    argv    += arg_idx - 1;

    if (cmd == CMD_PARSE) {
        stc_args_parse_exact(argc, argv, args + 1, 7, NULL);

        p   = parser_new(sdup(regex), parser_opts);
        res = parser_parse(p, &re);
        if (res.code == PARSE_SUCCESS) {
            regex_print_tree(re.root, outfile);
            regex_node_free(re.root);
        } else {
            fprintf(logfile, "ERROR %d: Invalidation of regex from %s\n",
                    res.code, res.ch);
            exit_code = res.code;
        }
        free((char *) p->regex);
        parser_free(p);
    } else if (cmd == CMD_COMPILE) {
        stc_args_parse_exact(argc, argv, args + 1, ARR_LEN(args) - 5, NULL);

        c = compiler_new(parser_new(sdup(regex), parser_opts), &compiler_opts);
        prog = compiler_compile(c);
        program_print(prog, outfile);

        compiler_free(c);
        program_free((Program *) prog);
    } else if (cmd == CMD_MATCH) {
        stc_args_parse_exact(argc, argv, args + 1, ARR_LEN(args) - 1, NULL);

        c = compiler_new(parser_new(sdup(regex), parser_opts), &compiler_opts);
        prog = compiler_compile(c);
        if (scheduler_type == SCH_SPENCER) {
            thread_manager = spencer_thread_manager_new(0, prog->thread_mem_len,
                                                        prog->ncaptures);
            if (compiler_opts.memo_scheme != MS_NONE)
                thread_manager = memoised_thread_manager_new(thread_manager);
        } else if (scheduler_type == SCH_LOCKSTEP) {
            thread_manager = thompson_thread_manager_new(
                0, prog->thread_mem_len, prog->ncaptures);
        }

        if (benchmark)
            thread_manager =
                benchmark_thread_manager_new(thread_manager, logfile);
        if (all_matches)
            thread_manager =
                all_matches_thread_manager_new(thread_manager, logfile, text);
        srvm = srvm_new(thread_manager, prog);
        while ((matched = srvm_find(srvm, text)) != MATCHES_EXHAUSTED) {
            fprintf(outfile, "matched = %d\n", matched);
            fprintf(outfile, "captures:\n");
            captures = srvm_captures(srvm, &ncaptures);
            fprintf(outfile, "  input: '%s'\n", text);
            for (i = 0; i < ncaptures; i++) {
                capture = captures[i];
                fprintf(outfile, "%7hu: ", i);
                if (capture.str) {
                    ncodepoints = stc_utf8_str_ncodepoints(text) -
                                  stc_utf8_str_ncodepoints(capture.str);
                    fprintf(outfile, "%*s'" STC_SV_FMT "'\n", (int) ncodepoints,
                            "", STC_SV_ARG(capture));
                } else {
                    fprintf(outfile, "not captured\n");
                }
            }
            free(captures);
        }

        compiler_free(c);
        program_free((Program *) prog);
        srvm_free(srvm);
    }

    if (logfile && logfile != stderr && logfile != stdout) fclose(logfile);
    if (outfile && outfile != stderr && outfile != stdout) fclose(outfile);

    return exit_code;
}
