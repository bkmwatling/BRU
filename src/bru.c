#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stc/fatp/string_view.h"
#include "stc/util/argparser.h"
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

typedef enum { SCH_SPENCER, SCH_LOCKSTEP } SchedulerType;

typedef struct {
    const char   *regex;
    const char   *text;
    const char   *cmd;
    int           benchmark;
    int           all_matches;
    FILE         *outfile;
    FILE         *logfile;
    SchedulerType scheduler_type;
    CompilerOpts  compiler_opts;
    ParserOpts    parser_opts;
} BruOptions;

static char *sdup(const char *s)
{
    char  *str;
    size_t len = strlen(s) + 1;

    str = malloc(len * sizeof(char));
    memcpy(str, s, len * sizeof(char));

    return str;
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

static void add_parsing_args(StcArgParser *ap, BruOptions *options)
{
    stc_argparser_add_str_argument(ap, "<regex>", "the regex to work with",
                                   &options->regex);
    stc_argparser_add_bool_option(
        ap, NULL, "--only-counters",
        "whether to use just counters and treat *, +, and ? as counters",
        &options->parser_opts.only_counters, FALSE);
    stc_argparser_add_bool_option(
        ap, "-u", "--unbounded-counters",
        "whether to permit unbounded counters or substitute with *",
        &options->parser_opts.unbounded_counters, FALSE);
    stc_argparser_add_bool_option(
        ap, "-e", "--expand-counters",
        "whether to expand counters with concatenation and nested ?",
        &options->parser_opts.expand_counters, FALSE);
    stc_argparser_add_bool_option(
        ap, "-w", "--whole-match-capture",
        "whether to have the whole regex match be the 0th capture",
        &options->parser_opts.whole_match_capture, FALSE);
}

static void add_compilation_args(StcArgParser *ap, BruOptions *options)
{
    stc_argparser_add_custom_option(
        ap, "-c", "--construction", "thompson | glushkov",
        "whether to compile using thompson or glushkov",
        &options->compiler_opts.construction, "thompson", convert_construction);
    stc_argparser_add_bool_option(
        ap, NULL, "--only-std-split",
        "whether to use standard `split` instruction only",
        &options->compiler_opts.only_std_split, FALSE);
    stc_argparser_add_custom_option(
        ap, NULL, "--capture-semantics", "pcre | re2",
        "which type of capturing semantics to compile with",
        &options->compiler_opts.capture_semantics, "pcre",
        convert_capture_semantics);
    stc_argparser_add_custom_option(
        ap, "-m", "--memo-scheme", "none | cn | in | iar",
        "which memoisation scheme to apply",
        &options->compiler_opts.memo_scheme, "none", convert_memo_scheme);
}

static void add_matching_args(StcArgParser *ap, BruOptions *options)
{
    stc_argparser_add_custom_option(
        ap, "-s", "--scheduler", "spencer | lockstep | thompson",
        "which scheduler to use for execution", &options->scheduler_type,
        "spencer", convert_scheduler_type);
    stc_argparser_add_bool_option(ap, "-b", "--benchmark",
                                  "whether to benchmark SRVM execution",
                                  &options->benchmark, FALSE);
    stc_argparser_add_bool_option(ap, NULL, "--all-matches",
                                  "whether to report all matches",
                                  &options->all_matches, FALSE);
    stc_argparser_add_str_argument(
        ap, "<input>", "the input string to match against the regex",
        &options->text);
}

static StcArgParser *setup_argparser(BruOptions *options)
{
    StcSubArgParsers *saps;
    StcArgParser     *parse, *compile, *match;
    StcArgParser     *ap = stc_argparser_new(NULL);

    stc_argparser_add_custom_option(
        ap, "-o", "--outfile", "filepath | stdout | stderr",
        "the file for normal output", &options->outfile, "stdout",
        convert_filepath);
    stc_argparser_add_custom_option(
        ap, "-l", "--logfile", "filepath | stdout | stderr",
        "the file for logging output", &options->logfile, "stderr",
        convert_filepath);

    saps = stc_argparser_add_subparsers(ap, "<subcommand>", &options->cmd);

    // parse
    parse = stc_subargparsers_add_argparser(
        saps, "parse", "parse the regex into a regex abstract syntax tree",
        NULL);
    add_parsing_args(parse, options);

    // compile
    compile = stc_subargparsers_add_argparser(
        saps, "compile", "compile the regex into a regex program", NULL);
    add_parsing_args(compile, options);
    add_compilation_args(compile, options);

    // match
    match = stc_subargparsers_add_argparser(
        saps, "match", "match the regex against an input string", NULL);
    add_parsing_args(match, options);
    add_compilation_args(match, options);
    add_matching_args(match, options);

    return ap;
}

int main(int argc, const char **argv)
{
    int            matched, exit_code = EXIT_SUCCESS;
    Parser        *p;
    Compiler      *c;
    Regex          re;
    ParseResult    res;
    const Program *prog;
    ThreadManager *thread_manager = NULL;
    SRVM          *srvm;
    StcStringView  capture, *captures;
    len_t          i, ncaptures;
    size_t         ncodepoints;
    BruOptions     options = { 0 };
    StcArgParser  *argparser;

    argparser = setup_argparser(&options);
    stc_argparser_parse(argparser, argc, argv);
    stc_argparser_free(argparser);

    if (strcmp(options.cmd, "parse") == 0) {
        p   = parser_new(sdup(options.regex), options.parser_opts);
        res = parser_parse(p, &re);
        if (res.code == PARSE_SUCCESS) {
            regex_print_tree(re.root, options.outfile);
            regex_node_free(re.root);
        } else {
            fprintf(options.logfile,
                    "ERROR %d: Invalidation of regex from %s\n", res.code,
                    res.ch);
            exit_code = res.code;
        }
        free((char *) p->regex);
        parser_free(p);
    } else if (strcmp(options.cmd, "compile") == 0) {
        c = compiler_new(parser_new(sdup(options.regex), options.parser_opts),
                         options.compiler_opts);
        prog = compiler_compile(c);
        program_print(prog, options.outfile);

        compiler_free(c);
        program_free((Program *) prog);
    } else if (strcmp(options.cmd, "match") == 0) {
        c = compiler_new(parser_new(sdup(options.regex), options.parser_opts),
                         options.compiler_opts);
        prog = compiler_compile(c);
        if (options.scheduler_type == SCH_SPENCER)
            thread_manager = spencer_thread_manager_new(0, prog->thread_mem_len,
                                                        prog->ncaptures);
        else if (options.scheduler_type == SCH_LOCKSTEP)
            thread_manager = thompson_thread_manager_new(
                0, prog->thread_mem_len, prog->ncaptures);

        if (options.compiler_opts.memo_scheme != MS_NONE)
            thread_manager = memoised_thread_manager_new(thread_manager);
        if (options.benchmark)
            thread_manager =
                benchmark_thread_manager_new(thread_manager, options.logfile);
        if (options.all_matches)
            thread_manager = all_matches_thread_manager_new(
                thread_manager, options.logfile, options.text);

        srvm = srvm_new(thread_manager, prog);
        while ((matched = srvm_find(srvm, options.text)) != MATCHES_EXHAUSTED) {
            fprintf(options.outfile, "matched = %d\n", matched);
            fprintf(options.outfile, "captures:\n");
            captures = srvm_captures(srvm, &ncaptures);
            fprintf(options.outfile, "  input: '%s'\n", options.text);
            for (i = 0; i < ncaptures; i++) {
                capture = captures[i];
                fprintf(options.outfile, "%7hu: ", i);
                if (capture.str) {
                    ncodepoints = stc_utf8_str_ncodepoints(options.text) -
                                  stc_utf8_str_ncodepoints(capture.str);
                    fprintf(options.outfile, "%*s'" STC_SV_FMT "'\n",
                            (int) ncodepoints, "", STC_SV_ARG(capture));
                } else {
                    fprintf(options.outfile, "not captured\n");
                }
            }
            free(captures);
        }

        compiler_free(c);
        program_free((Program *) prog);
        srvm_free(srvm);
    }

    if (options.logfile && options.logfile != stderr &&
        options.logfile != stdout)
        fclose(options.logfile);
    if (options.outfile && options.outfile != stderr &&
        options.outfile != stdout)
        fclose(options.outfile);

    return exit_code;
}
