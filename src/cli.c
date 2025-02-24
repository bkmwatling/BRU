#include <stdlib.h>

#include <bru/cli.h>
#include <bru/fa/constructions/glushkov.h>
#include <bru/fa/constructions/thompson.h>
#include <bru/fa/transformers/flatten.h>
#include <bru/fa/transformers/memoisation.h>
#include <bru/fa/transformers/path_encoding.h>
#include <bru/utils.h>
#include <bru/vm/compilers/smir.h>
#include <bru/vm/srvm.h>
#include <bru/vm/thread/managers/benchmark.h>
#include <bru/vm/thread/managers/captures.h>
#include <bru/vm/thread/managers/counters.h>
#include <bru/vm/thread/managers/lockstep.h>
#include <bru/vm/thread/managers/memoisation.h>
#include <bru/vm/thread/managers/memory.h>
#include <bru/vm/thread/managers/pool.h>
#include <bru/vm/thread/managers/spencer.h>
#include <bru/vm/thread/managers/write.h>

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

static StcArgConvertResult convert_optimise_level(const char *arg, void *out)
{
    BruOptimisationLevel *ol = out;

    if (strcmp(arg, "0") == 0 || strcmp(arg, "none") == 0)
        *ol = BRU_OPTIMISE_NONE;
    else if (strcmp(arg, "1") == 0 || strcmp(arg, "full") == 0)
        *ol = BRU_OPTIMISE_FULL;
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
                                   &options->parse.regex);
    stc_argparser_add_bool_option(
        ap, NULL, "--only-counters",
        "whether to use just counters and treat *, +, and ? as counters",
        &options->parse.opts.only_counters, FALSE);
    stc_argparser_add_bool_option(
        ap, "-u", "--unbounded-counters",
        "whether to permit unbounded counters or substitute with *",
        &options->parse.opts.unbounded_counters, FALSE);
    stc_argparser_add_bool_option(
        ap, "-e", "--expand-counters",
        "whether to expand counters with concatenation and nested ?",
        &options->parse.opts.expand_counters, FALSE);
    stc_argparser_add_bool_option(
        ap, "-w", "--whole-match-capture",
        "whether to have the whole regex match be the 0th capture",
        &options->parse.opts.whole_match_capture, FALSE);
    stc_argparser_add_bool_option(
        ap, NULL, "--log-unsupported",
        "whether to log unsupported features in the regex",
        &options->parse.opts.log_unsupported, FALSE);
    stc_argparser_add_bool_option(
        ap, NULL, "--flag-problematic",
        "whether to flag expressions like E* with E matching epsilon",
        &options->parse.opts.allow_repeated_nullability, TRUE);
}

static void add_compilation_args(StcArgParser *ap, BruOptions *options)
{
    stc_argparser_add_custom_option(
        ap, "-c", "--construction", "thompson | glushkov | flat",
        "whether to compile using thompson, glushkov, or flat",
        &options->compile.pipeline.construction, "thompson",
        convert_construction);
    stc_argparser_add_bool_option(
        ap, NULL, "--only-std-split",
        "whether to use standard `split` instruction only",
        &options->compile.pipeline.only_std_split, FALSE);
    stc_argparser_add_custom_option(
        ap, NULL, "--capture-semantics", "pcre | re2",
        "which type of capturing semantics to compile with",
        &options->compile.pipeline.construction_opts.capture_semantics, "pcre",
        convert_capture_semantics);
    stc_argparser_add_custom_option(
        ap, "-m", "--memo-scheme", "none | cn | in | iar",
        "which memoisation scheme to apply",
        &options->compile.pipeline.memo_scheme, "none", convert_memo_scheme);
    stc_argparser_add_bool_option(
        ap, NULL, "--mark-states",
        "whether to compile state marking instructions",
        &options->compile.pipeline.mark_states, FALSE);
    stc_argparser_add_bool_option(
        ap, NULL, "--encode-priorities",
        "whether to encode transition priorities on the transitions",
        &options->compile.pipeline.encode_priorities, FALSE);
    stc_argparser_add_custom_option(
        ap, "-O", "--optimise", "none | 0 | full | 1",
        "set the level of compiled code optimisation",
        &options->compile.pipeline.optimisation_level, "full",
        convert_optimise_level);
    stc_argparser_add_bool_option(
        ap, NULL, "--state-machine",
        "output the state machine instead of the program",
        &options->compile.only_state_machine, FALSE);
}

static void add_matching_args(StcArgParser *ap, BruOptions *options)
{
    stc_argparser_add_custom_option(
        ap, "-s", "--scheduler", "spencer | lockstep | thompson",
        "which scheduler to use for execution", &options->match.scheduler_type,
        "spencer", convert_scheduler_type);
    stc_argparser_add_bool_option(
        ap, "-b", "--benchmark",
        "whether to benchmark SRVM execution, writing to the logfile",
        &options->match.benchmark, FALSE);
    stc_argparser_add_bool_option(ap, NULL, "--no-pool",
                                  "disable thread pool usage (no thread reuse)",
                                  &options->match.thread_pool, TRUE);
    // NOTE: deprecated/not useful, see all_matches ThreadManager
    // stc_argparser_add_bool_option(ap, NULL, "--all-matches",
    //                               "whether to report all matches",
    //                               &options->all_matches, FALSE);
    stc_argparser_add_str_argument(
        ap, "<input>", "the input string to match against the regex",
        &options->match.text);
}

/* --- Helper functions ----------------------------------------------------- */

static char *sdup(const char *s)
{
    char  *str;
    size_t len = strlen(s) + 1;

    str = malloc(len * sizeof(char));
    memcpy(str, s, len * sizeof(char));

    return str;
}

static void compile_state_markers(void                   *meta,
                                  StcVec(BruInstruction) *instructions)
{
    BRU_UNUSED(meta);
    PUSH_INSTRUCTION(instructions, .bytecode = BRU_STATE);
}

/* --- API function definitions --------------------------------------------- */

StcArgParser *bru_cli_setup_argparser(BruOptions *options)
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

BruParser *bru_cli_make_parser(BruOptions *options)
{
    return bru_parser_new(sdup(options->parse.regex), options->parse.opts);
}

BruStateMachine *bru_cli_make_state_machine(BruOptions *options,
                                            BruParser  *parser)
{
    BruRegex         re;
    BruParseResult   res;
    BruStateMachine *sm = NULL, *tmp = NULL;

    res = bru_parser_parse(parser, &re);
    if (res.code != BRU_PARSE_SUCCESS) return NULL;

    switch (options->compile.pipeline.construction) {
        case BRU_FLAT:
        case BRU_THOMPSON:
            sm = bru_thompson_construct(
                re, options->compile.pipeline.construction_opts);
            break;
        case BRU_GLUSHKOV:
            sm = bru_glushkov_construct(
                re, options->compile.pipeline.construction_opts);
            break;
    }
    bru_regex_node_free(re.root);

    if (options->compile.pipeline.encode_priorities)
        sm = bru_transform_path_encode(sm);

    if (options->compile.pipeline.memo_scheme != BRU_MS_NONE)
        sm = bru_transform_memoise(sm, options->compile.pipeline.memo_scheme,
                                   parser->opts.logfile);
    if (options->compile.pipeline.construction == BRU_FLAT) {
        tmp = bru_transform_flatten(sm, parser->opts.logfile);
        bru_smir_free(sm);
        sm = tmp;
    }

    return sm;
}

BruProgram *bru_cli_make_program(BruOptions *options, BruParser *parser)
{
    BruStateMachine       *sm;
    StcVec(BruInstruction) instructions;
    BruProgram            *prog;

    sm = bru_cli_make_state_machine(options, parser);
    if (!sm) return NULL;

    instructions = bru_smir_compile_with_meta(
        sm,
        options->compile.pipeline.mark_states ? compile_state_markers : NULL,
        NULL);
    prog = bru_compiler_compile(parser->regex, instructions,
                                options->compile.pipeline.optimisation_level);

    bru_smir_free(sm);
    stc_vec_free(instructions);

    return prog;
}

BruThreadManager *bru_cli_make_thread_manager(BruOptions       *options,
                                              const BruProgram *prog)
{
    BruThreadManager *thread_manager;

    if (options->match.scheduler_type == SCH_SPENCER)
        thread_manager = bru_spencer_thread_manager_new();
    else if (options->match.scheduler_type == SCH_LOCKSTEP)
        thread_manager = bru_lockstep_thread_manager_new();

    if (prog->ncaptures)
        thread_manager = bru_thread_manager_with_captures_new(thread_manager,
                                                              prog->ncaptures);
    if (prog->ncounters)
        thread_manager = bru_thread_manager_with_counters_new(thread_manager,
                                                              prog->ncounters);
    if (prog->thread_mem_len)
        thread_manager = bru_thread_manager_with_memory_new(
            thread_manager, prog->thread_mem_len);
    if (prog->requires_writing)
        thread_manager = bru_thread_manager_with_write_new(thread_manager);
    if (options->match.thread_pool)
        thread_manager =
            bru_thread_manager_with_pool_new(thread_manager, options->logfile);
    if (options->compile.pipeline.memo_scheme != BRU_MS_NONE)
        thread_manager = bru_memoised_thread_manager_new(thread_manager);
    if (options->match.benchmark)
        thread_manager =
            bru_benchmark_thread_manager_new(thread_manager, options->logfile);

    return thread_manager;
}
