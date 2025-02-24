#ifndef BRU_CLI_H
#define BRU_CLI_H

#include <stc/util/argparser.h>

#include <bru/fa/constructions/opts.h>
#include <bru/fa/smir.h>
#include <bru/fa/transformers/opts.h>
#include <bru/re/parser.h>
#include <bru/vm/compiler.h>

/* --- Data structures ------------------------------------------------------ */

typedef enum { BRU_THOMPSON, BRU_GLUSHKOV, BRU_FLAT } BruConstruction;

typedef enum { SCH_BACKTRACK, SCH_LOCKSTEP } SchedulerType;

typedef struct {
    BruConstruction      construction;
    BruConstructionOpts  construction_opts;
    BruMemoScheme        memo_scheme;
    int                  only_std_split;
    int                  mark_states;
    int                  encode_priorities;
    BruOptimisationLevel optimisation_level;
} BruCompilationPipeline;

typedef struct {
    const char   *regex;
    BruParserOpts opts;
} BruCLIParseOpts;

typedef struct {
    BruCompilationPipeline pipeline;
    int                    only_state_machine;
} BruCLICompileOpts;

typedef struct {
    const char   *text;
    int           benchmark;
    int           thread_pool;
    int           all_matches;
    SchedulerType scheduler_type;
} BruCLIMatchOpts;

typedef struct {
    size_t            cmd;
    FILE             *outfile;
    FILE             *logfile;
    BruCLIParseOpts   parse;
    BruCLICompileOpts compile;
    BruCLIMatchOpts   match;
} BruOptions;

/* --- CLI function prototypes ---------------------------------------------- */

/**
 * Create an argument parser for BRU's command line interface.
 *
 * @param[out] options where the parser will store the parsed arguments
 *
 * @return the created argument parser
 */
StcArgParser *bru_cli_setup_argparser(BruOptions *options);

/**
 * Create the regex parser from the parsed command line options.
 *
 * @param[in] options the parsed command line options
 *
 * @return the constructed regex parser
 */
BruParser *bru_cli_make_parser(BruOptions *options);

/**
 * Construct a state machine according to the parsed command line options.
 *
 * @param[in] options the parsed command line options
 * @param[in] parser  the parser for parsing the regex
 *
 * @return the constructed state machine
 */
BruStateMachine *bru_cli_make_state_machine(BruOptions *options,
                                            BruParser  *parser);

/**
 * Compile a program according to the parsed command line options.
 *
 * @param[in] options the parsed command line options
 * @param[in] parser  the parser for parsing the regex
 *
 * @return the compiled program
 */
BruProgram *bru_cli_make_program(BruOptions *options, BruParser *parser);

/**
 * Create a thread manager according to the parsed command line options.
 *
 * The program must be compiled first in order to determine if extra features
 * are required (such as counters, thread memory, or captures).
 *
 * @param[in] options the parsed command line options
 * @param[in] prog    the compiled program to execute the thread manager with
 *
 * @return the constructed thread manager
 */
BruThreadManager *bru_cli_make_thread_manager(BruOptions       *options,
                                              const BruProgram *prog);

#endif /* BRU_CLI_H */
