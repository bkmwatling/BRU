#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stc/fatp/str_view.h>
#include <stc/util/utf.h>

#include <bru/cli.h>

/* --- Subcommand execution functions --------------------------------------- */

static int parse(BruOptions *options)
{
    BruParser     *p;
    BruParseResult res;
    BruRegex       re;
    int            exit_code = 0;

    p   = bru_cli_make_parser(options);
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
    BruParser       *parser;
    BruProgram      *prog      = NULL;
    int              exit_code = EXIT_SUCCESS;
    BruStateMachine *sm        = NULL;

    parser = bru_cli_make_parser(options);
    if (options->compile.only_state_machine) {
        sm = bru_cli_make_state_machine(options, parser);
        if (sm) {
            bru_smir_print(sm, options->outfile);
            bru_smir_free(sm);
        }
    } else {
        prog = bru_cli_make_program(options, parser);
        if (prog) {
            bru_program_print(prog, options->outfile);
            bru_program_free(prog);
        }
    }

    if (!sm && !prog) {
        fputs("ERROR: compilation failed\n", stderr);
        exit_code = EXIT_FAILURE;
    }

    bru_parser_free(parser);
    return exit_code;
}

static int match(BruOptions *options)
{
    BruParser        *parser;
    BruProgram       *prog;
    BruThreadManager *thread_manager = NULL;
    BruSRVM          *srvm;
    StcStrView        capture;
    bru_len_t         i;
    size_t            ncodepoints;
    BruSRVMMatch     *match;
    int               exit_code = EXIT_SUCCESS;

    parser = bru_cli_make_parser(options);
    prog   = bru_cli_make_program(options, parser);
    if (prog == NULL) {
        fputs("ERROR: compilation failed\n", stderr);
        exit_code = EXIT_FAILURE;
        goto done;
    }

    thread_manager = bru_cli_make_thread_manager(options, prog);
    srvm           = bru_srvm_new(thread_manager, prog);
    if (!(match = bru_srvm_find(srvm, options->match.text)))
        fputs("No match\n", options->outfile);
    else
        do {
            fprintf(options->outfile,
                    "Found match\n"
                    "bytes: %.*s\n"
                    "captures:\n"
                    "  input: '%s'\n",
                    (int) match->nbytes, match->bytes, options->match.text);
            for (i = 0; i < match->ncaptures; i++) {
                capture = match->captures[i];
                fprintf(options->outfile, "%7hu: ", i);
                if (capture.str) {
                    ncodepoints =
                        stc_utf8_str_ncodepoints(options->match.text) -
                        stc_utf8_str_ncodepoints(capture.str);
                    fprintf(options->outfile, "%*s'" STC_SV_FMT "'\n",
                            (int) ncodepoints, "", STC_SV_ARG(capture));
                } else {
                    fprintf(options->outfile, "not captured\n");
                }
            }
            bru_srvm_match_free(match);
        } while ((match = bru_srvm_find(srvm, options->match.text)));
    bru_program_free(prog);
    bru_srvm_free(srvm);

done:
    bru_parser_free(parser);
    return exit_code;
}

int main(int argc, const char **argv)
{
    int           exit_code;
    StcArgParser *argparser;
    BruOptions    options                     = { 0 };
    static int (*subcommands[])(BruOptions *) = { parse, compile, match };

    argparser = bru_cli_setup_argparser(&options);
    stc_argparser_parse(argparser, argc, argv);
    stc_argparser_free(argparser);

    options.parse.opts.logfile = options.logfile;
    exit_code                  = subcommands[options.cmd](&options);

    if (options.logfile && options.logfile != stderr &&
        options.logfile != stdout)
        fclose(options.logfile);
    if (options.outfile && options.outfile != stderr &&
        options.outfile != stdout)
        fclose(options.outfile);

    return exit_code;
}
