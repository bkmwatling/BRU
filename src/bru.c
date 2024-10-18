#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stc/fatp/string_view.h>
#include <stc/util/argparser.h>
#include <stc/util/utf.h>

#include "re/parser.h"
#include "vm/compiler.h"
#include "vm/srvm.h"
// NOTE: deprecated/not useful, see all_matches ThreadManager
// #include "vm/thread_managers/all_matches.h"
#include "vm/thread_managers/benchmark.h"
#include "vm/thread_managers/captures.h"
#include "vm/thread_managers/counters.h"
#include "vm/thread_managers/lockstep.h"
#include "vm/thread_managers/memoisation.h"
#include "vm/thread_managers/memory.h"
#include "vm/thread_managers/spencer.h"
#include "vm/thread_managers/thread_pool.h"
#include "vm/thread_managers/write.h"

#define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

typedef enum { SCH_SPENCER, SCH_LOCKSTEP } SchedulerType;

typedef struct {
    const char     *regex;
    const char     *text;
    size_t          cmd;
    int             benchmark;
    int             all_matches;
    FILE           *outfile;
    FILE           *logfile;
    SchedulerType   scheduler_type;
    BruCompilerOpts compiler_opts;
    BruParserOpts   parser_opts;
} BruOptions;

static char *sdup(const char *s)
{
    char  *str;
    size_t len = strlen(s) + 1;

    str = malloc(len * sizeof(char));
    memcpy(str, s, len * sizeof(char));

    return str;
}

/* --- Command-line argument functions -------------------------------------- */

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
    BruConstruction *construction = out;

    if (strcmp(arg, "thompson") == 0)
        *construction = BRU_THOMPSON;
    else if (strcmp(arg, "glushkov") == 0)
        *construction = BRU_GLUSHKOV;
    else if (strcmp(arg, "flat") == 0)
        *construction = BRU_FLAT;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_capture_semantics(const char *arg, void *out)
{
    BruCaptureSemantics *cs = out;

    if (strcmp(arg, "pcre") == 0)
        *cs = BRU_CS_PCRE;
    else if (strcmp(arg, "re2") == 0)
        *cs = BRU_CS_RE2;
    else
        return STC_ARG_CR_FAILURE;

    return STC_ARG_CR_SUCCESS;
}

static StcArgConvertResult convert_memo_scheme(const char *arg, void *out)
{
    BruMemoScheme *ms = out;

    if (strcmp(arg, "in") == 0 || strcmp(arg, "IN") == 0)
        *ms = BRU_MS_IN;
    else if (strcmp(arg, "cn") == 0 || strcmp(arg, "CN") == 0)
        *ms = BRU_MS_CN;
    else if (strcmp(arg, "iar") == 0 || strcmp(arg, "IAR") == 0)
        *ms = BRU_MS_IAR;
    else if (strcmp(arg, "none") == 0)
        *ms = BRU_MS_NONE;
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
    stc_argparser_add_bool_option(
        ap, NULL, "--log-unsupported",
        "whether to log unsupported features in the regex",
        &options->parser_opts.log_unsupported, FALSE);
    stc_argparser_add_bool_option(
        ap, NULL, "--flag-problematic",
        "whether to flag expressions like E* with E matching epsilon",
        &options->parser_opts.allow_repeated_nullability, TRUE);
}

static void add_compilation_args(StcArgParser *ap, BruOptions *options)
{
    stc_argparser_add_custom_option(
        ap, "-c", "--construction", "thompson | glushkov | flat",
        "whether to compile using thompson, glushkov, or flat",
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
    stc_argparser_add_bool_option(
        ap, NULL, "--mark-states",
        "whether to compile state marking instructions",
        &options->compiler_opts.mark_states, FALSE);
    stc_argparser_add_bool_option(
        ap, NULL, "--encode-priorities",
        "whether to encode transition priorities on the transitions",
        &options->compiler_opts.encode_priorities, FALSE);
}

static void add_matching_args(StcArgParser *ap, BruOptions *options)
{
    stc_argparser_add_custom_option(
        ap, "-s", "--scheduler", "spencer | lockstep | thompson",
        "which scheduler to use for execution", &options->scheduler_type,
        "spencer", convert_scheduler_type);
    stc_argparser_add_bool_option(
        ap, "-b", "--benchmark",
        "whether to benchmark SRVM execution, writing to the logfile",
        &options->benchmark, FALSE);
    // NOTE: deprecated/not useful, see all_matches ThreadManager
    // stc_argparser_add_bool_option(ap, NULL, "--all-matches",
    //                               "whether to report all matches",
    //                               &options->all_matches, FALSE);
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

    saps =
        stc_argparser_add_subparsers(ap, "<subcommand>", NULL, &options->cmd);

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

/* --- Subcommand execution functions --------------------------------------- */

static int parse(BruOptions *options)
{
    BruParser     *p;
    BruParseResult res;
    BruRegex       re;
    int            exit_code = 0;

    p   = bru_parser_new(sdup(options->regex), options->parser_opts);
    res = bru_parser_parse(p, &re);
    if (res.code < BRU_PARSE_NO_MATCH) {
        bru_regex_print_tree(re.root, options->outfile);
        bru_regex_node_free(re.root);
        if (res.code != BRU_PARSE_SUCCESS) exit_code = EXIT_FAILURE;
    } else {
        fprintf(options->logfile, "ERROR %d: Invalidation of regex from %s\n",
                res.code, res.ch);
        exit_code = res.code;
    }
    free((char *) p->regex);
    bru_parser_free(p);

    return exit_code;
}

static int compile(BruOptions *options)
{
    BruCompiler      *c;
    const BruProgram *prog;
    int               exit_code = EXIT_SUCCESS;

    c = bru_compiler_new(
        bru_parser_new(sdup(options->regex), options->parser_opts),
        options->compiler_opts);
    prog = bru_compiler_compile(c);
    if (prog) {
        bru_program_print(prog, options->outfile);
        bru_program_free((BruProgram *) prog);
    } else {
        fputs("ERROR: compilation failed\n", stderr);
        exit_code = EXIT_FAILURE;
    }

    bru_compiler_free(c);

    return exit_code;
}

static int match(BruOptions *options)
{
    BruCompiler      *c;
    const BruProgram *prog;
    BruThreadManager *thread_manager = NULL;
    BruSRVM          *srvm;
    StcStringView     capture;
    bru_len_t         i;
    size_t            ncodepoints;
    BruSRVMMatch     *match;
    int               exit_code = EXIT_SUCCESS;

    c = bru_compiler_new(
        bru_parser_new(sdup(options->regex), options->parser_opts),
        options->compiler_opts);
    prog = bru_compiler_compile(c);
    if (prog == NULL) {
        fputs("ERROR: compilation failed\n", stderr);
        exit_code = EXIT_FAILURE;
        goto done;
    }

    if (options->scheduler_type == SCH_SPENCER)
        thread_manager = bru_spencer_thread_manager_new();
    else if (options->scheduler_type == SCH_LOCKSTEP)
        thread_manager = bru_lockstep_thread_manager_new();

    if (prog->ncaptures)
        thread_manager = bru_thread_manager_with_captures_new(thread_manager,
                                                              prog->ncaptures);
    // TODO
    // if (prog->counters)
    //     thread_manager = bru_thread_manager_with_counters_new(thread_manager,
    //                                                           prog->counters);

    if (prog->thread_mem_len)
        thread_manager = bru_thread_manager_with_memory_new(
            thread_manager, prog->thread_mem_len);

    if (prog->requires_writing)
        thread_manager = bru_thread_manager_with_write_new(thread_manager);

    // TODO: add command line flag for thread pool
    thread_manager =
        bru_thread_manager_with_pool_new(thread_manager, options->logfile);

    if (options->compiler_opts.memo_scheme != BRU_MS_NONE)
        thread_manager = bru_memoised_thread_manager_new(thread_manager);
    if (options->benchmark)
        thread_manager =
            bru_benchmark_thread_manager_new(thread_manager, options->logfile);
    // NOTE: deprecated/not useful, see all_matches ThreadManager
    // if (options->all_matches)
    //     thread_manager = all_matches_thread_manager_new(
    //         thread_manager, options->logfile, options->text);

    srvm = bru_srvm_new(thread_manager, prog);
    if (!(match = bru_srvm_find(srvm, options->text)))
        fputs("No match\n", options->outfile);
    else
        do {
            fprintf(options->outfile,
                    "Found match\n"
                    "bytes: %.*s\n"
                    "captures:\n"
                    "  input: '%s'\n",
                    (int) match->nbytes, match->bytes, options->text);
            for (i = 0; i < match->ncaptures; i++) {
                capture = match->captures[i];
                fprintf(options->outfile, "%7hu: ", i);
                if (capture.str) {
                    ncodepoints = stc_utf8_str_ncodepoints(options->text) -
                                  stc_utf8_str_ncodepoints(capture.str);
                    fprintf(options->outfile, "%*s'" STC_SV_FMT "'\n",
                            (int) ncodepoints, "", STC_SV_ARG(capture));
                } else {
                    fprintf(options->outfile, "not captured\n");
                }
            }
            bru_srvm_match_free(match);
        } while ((match = bru_srvm_find(srvm, options->text)));
    bru_program_free((BruProgram *) prog);
    bru_srvm_free(srvm);

done:
    bru_compiler_free(c);

    return exit_code;
}

int main(int argc, const char **argv)
{
    int           exit_code;
    StcArgParser *argparser;
    BruOptions    options                     = { 0 };
    static int (*subcommands[])(BruOptions *) = { parse, compile, match };

    argparser = setup_argparser(&options);
    stc_argparser_parse(argparser, argc, argv);
    stc_argparser_free(argparser);

    options.parser_opts.logfile = options.logfile;
    exit_code                   = subcommands[options.cmd](&options);

    if (options.logfile && options.logfile != stderr &&
        options.logfile != stdout)
        fclose(options.logfile);
    if (options.outfile && options.outfile != stderr &&
        options.outfile != stdout)
        fclose(options.outfile);

    return exit_code;
}
