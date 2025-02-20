/* Regular expression tests.
   Copyright (C) 2003-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include "testing_utils.h"

static int check_match(const BruSRVMMatch *match, const BruSRVMMatch *expected)
{
    if (!expected) return expected == match;

    if (expected->ncaptures != match->ncaptures) return 0;

    for (int i = 0; i < expected->ncaptures; i++) {
        if (match->captures[i].str != expected->captures[i].str ||
            match->captures[i].len != expected->captures[i].len) {
            return 0;
        }
    }

    return 1;
}

static TestResult *timed_find_match(void *arg_container)
{
    struct {
        BruOptions    *options;
        BruSRVMMatch **match;
    } *arg = arg_container;

    TestResult *res = malloc(sizeof(TestResult));

    *res = find_match(arg->options, arg->match);
    return res;
}

static void log_result(FILE               *result_log,
                       int                 passed,
                       const char         *regex,
                       const char         *input,
                       const char         *other_args,
                       TestResult          expected_res,
                       TestResult          observed,
                       const BruSRVMMatch *expected_match,
                       const BruSRVMMatch *match)
{
    char *pcre2_match_info_field = match_info_field(expected_match, input),
         *bru_match_info_field   = match_info_field(match, input),
         *regex_field            = str_to_field(regex, REPLACE_SPECIAL_CHARS),
         *input_field            = str_to_field(input, REPLACE_SPECIAL_CHARS),
         *other_args_field =
             str_to_field(other_args, DO_NOT_REPLACE_SPECIAL_CHARS);

    fprintf(result_log, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
            passed ? "PASS" : "FAIL", regex_field, input_field,
            other_args_field, test_result_str[expected_res],
            test_result_str[observed], pcre2_match_info_field,
            bru_match_info_field);

    free(pcre2_match_info_field);
    free(bru_match_info_field);
    free(regex_field);
    free(input_field);
    free(other_args_field);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <testfile> <result_log>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *f = fopen(argv[1], "r");

    if (f == NULL) {
        fprintf(stderr, "Couldn't open %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    FILE *result_log = fopen(argv[2], "w");
    if (result_log == NULL) {
        fprintf(stderr, "Couldn't open %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    fprintf(result_log, "result\tregex\tinput\tother_bru_args\t"
                        "pcre2_outcome\tbru_outcome\t"
                        "pcre2_match_info\tbru_match_info\n");

    size_t buf_len  = 100;
    char  *line_buf = malloc(buf_len * sizeof(char));
    size_t line_len = 0;

    int test_num = 0;

    unsigned int n_diffs = 0, n_sim_diffs = 0,
                 n_unexpected_compilation_failures  = 0,
                 n_unexpected_compilation_successes = 0, n_timeouts = 0;

    while (read_line(&line_buf, &buf_len, f, &line_len)) {
        const char *line = strip(line_buf, &line_len);

        // Skip comments and empty lines.
        if (*line == '#' || *line == '\0') continue;

        test_num++;

        if (test_num == 1) {
            // Skip the header line.
            continue;
        }

        char *regex, *input, *other_args, *expected_outcome,
            *expected_match_info;
        parse_input_fields(line, &regex, &input, &other_args, &expected_outcome,
                           &expected_match_info);

        int   bru_argc;
        char *arg_buf = malloc(strlen(other_args) + 1);
        strcpy(arg_buf, other_args);
        const char **bru_argv = new_bru_argv(&bru_argc, regex, input, arg_buf);

        BruSRVMMatch *expected_match = NULL;

        TestResult expected_res = test_result(expected_outcome);

        int n_groups = *expected_match_info == '\0'
                           ? 0
                           : 1 + count(' ', expected_match_info);
        if (expected_res == MATCH) {
            expected_match            = malloc(sizeof(BruSRVMMatch));
            expected_match->ncaptures = n_groups;
            expected_match->captures  = calloc(n_groups, sizeof(StcStrView));
        }

        BruOptions    options   = { 0 };
        StcArgParser *argparser = bru_cli_setup_argparser(&options);
        stc_argparser_parse(argparser, bru_argc, bru_argv);
        stc_argparser_free(argparser);
        options.parse.opts.whole_match_capture = TRUE;
        options.parse.opts.expand_counters     = TRUE;

        if (expected_res == MATCH) {
            char *bounds[n_groups];
            bounds[0] = strtok(expected_match_info, " ");
            for (int i = 1; i < n_groups; i++) bounds[i] = strtok(NULL, " ");
            for (int i = 0; i < n_groups; i++) {
                long start = atoi(strtok(bounds[i], ":"));
                long end   = atoi(strtok(NULL, ":"));
                assert(start >= 0 || start == -1);
                assert(end >= 0 || end == -1);
                if (start == -1 || end == -1) {
                    expected_match->captures[i].str = NULL;
                    expected_match->captures[i].len = 0;
                } else {
                    expected_match->captures[i].str =
                        options.match.text + start;
                    expected_match->captures[i].len = end - start;
                }
            }
        }

        BruSRVMMatch *match = NULL;

        pthread_t thread;

        struct {
            BruOptions    *options;
            BruSRVMMatch **match;
        } arg;

        arg.options = &options;
        arg.match   = &match;
        TestResult observed;

        if (pthread_create(&thread, NULL,
                           (void *(*) (void *) ) & timed_find_match,
                           &arg) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        // Get the result or time out
        struct timespec time_limit;
        if (clock_gettime(CLOCK_REALTIME, &time_limit) == -1) {
            perror("clock_gettime");
            exit(EXIT_FAILURE);
        }
        // Memory leak if thread times out (none of its heap allocated memory is
        // freed)
        time_limit.tv_sec         += 5;
        int         err            = 0;
        TestResult *thread_return  = NULL;
        if ((err = pthread_timedjoin_np(thread, (void **) &thread_return,
                                        &time_limit)) != 0 &&
            err != ETIMEDOUT) {
            perror("error in pthread_timedjoin_np");
            printf("%d\n", err);
            exit(EXIT_FAILURE);
        } else if (err == ETIMEDOUT) {
            observed = TIMEOUT;
            n_timeouts++;
        } else {
            assert(err == 0);
            assert(thread_return != NULL);
            observed = *thread_return;
            free(thread_return);
            thread_return = NULL;
        }
        assert(thread_return == NULL);

        int passed = 0;
        if (observed == expected_res) {
            if (expected_res == MATCH) {
                passed = check_match(match, expected_match);
                if (!passed) {
                    n_sim_diffs += 1;
                    n_diffs     += 1;
                }
            } else
                passed = 1;
        } else {
            n_diffs += 1;
            if (expected_res == COMPILATION_FAILURE)
                n_unexpected_compilation_successes += 1;
            else if (observed == COMPILATION_FAILURE)
                n_unexpected_compilation_failures += 1;
            else
                n_sim_diffs += 1;
        }
        // XXX: match_info_field allocates memory that is not freed.

        log_result(result_log, passed, regex, input, other_args, expected_res,
                   observed, expected_match, match);

        free(bru_argv);
        free(arg_buf);
        free(regex);
        free(input);
        free(other_args);
        if (expected_match) {
            free(expected_match->captures);
            free(expected_match);
        }
        if (match) {
            free(match->captures);
            free(match);
        }
        if (expected_match_info) free(expected_match_info);
        if (expected_outcome) free(expected_outcome);
        /*

    *regex            = parse_field(mutable_line, "\t", REPLACE_SPECIAL_CHARS);
    *input            = parse_field(NULL, "\t", REPLACE_SPECIAL_CHARS);
    *other_args       = parse_field(NULL, "\t", DO_NOT_REPLACE_SPECIAL_CHARS);
    *expected_outcome = parse_field(NULL, "\t", DO_NOT_REPLACE_SPECIAL_CHARS);
    *expected_match_info       = parse_field(NULL, "\t",
    DO_NOT_REPLACE_SPECIAL_CHARS);
        */
    }

    printf("Number of tests: %d\nNumber of differences: %d\nNumber of "
           "simulation differences: %d\nNumber of unexpected compilation "
           "failures: %d\nNumber of unexpected compilation successes "
           "%d\nNumber of timeouts: %d\n",
           test_num, n_diffs, n_sim_diffs, n_unexpected_compilation_failures,
           n_unexpected_compilation_successes, n_timeouts);

    free(line_buf);
    fclose(f);
    fclose(result_log);
    return EXIT_SUCCESS;
}
