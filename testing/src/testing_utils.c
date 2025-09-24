#include "testing_utils.h"
#include <assert.h>
#include <stdlib.h>
#define _GNU_SOURCE

#include <string.h>

#define MIN_BRU_ARGC 5

const char *test_result_str[] = { "SETUP_FAILURE",  "COMPILATION_FAILURE",
                                  "MATCHING_ERROR", "MATCH",
                                  "NO_MATCH",       "TIMEOUT" };

#define max(a, b) ((a) > (b) ? (a) : (b))

short read_line(char **line_buf, size_t *buf_len, FILE *f, size_t *line_len)
{
    assert(*line_buf && *buf_len > 0);
    char   c;
    size_t pos = 0;
    while ((c = fgetc(f)) != '\n' && c != EOF) {
        if (pos >= (*buf_len - 2)) {
            *buf_len  = (*buf_len) << 1;
            // clang-format off
            #ifdef DEBUG_READ_LINE
                printf("# reallocating to %ld\n", *buf_len);
            #endif
            // clang-format on
            *line_buf = realloc(*line_buf, (*buf_len) * sizeof(char));
        }
        (*line_buf)[pos++] = c;
    }
    (*line_buf)[pos] = '\0';
    *line_len        = pos;
    return c != EOF;
}

int count_args(const char *s)
{
    assert(s);
    if (*s == '\0') return 0;
    int count = 1;
    for (const char *p = s; *p; p++)
        if ((*p == ' ' || *p == '\t')) {
            count++;
            while (*p == ' ' || *p == '\t') p++;
        }
    return count;
}

const char **new_bru_argv(int        *bru_argc_out,
                          const char *regex,
                          const char *input,
                          char       *arg_buf)
{
    int n_args            = count_args(arg_buf);
    // + 5 for the executable name, subcommand, '--', regex, and input
    // (see `init_bru_argv`)
    *bru_argc_out         = n_args + MIN_BRU_ARGC;
    const char **bru_argv = calloc(*bru_argc_out, sizeof(char *));
    init_bru_argv(*bru_argc_out, bru_argv, regex, input, arg_buf);
    // clang-format off
    #define DEBUG_ARGV
    #ifdef DEBUG_ARGV
        printf("# bru_argv: [ ");
        for (int i = 0; i < *bru_argc_out; i++)
            printf("\"%s\"%s ", bru_argv[i], i < *bru_argc_out - 1 ? "," : "");
        printf("]\n");
    #endif
    // clang-format on
    return bru_argv;
}

void init_bru_argv(int          bru_argc,
                   const char **bru_argv_out,
                   const char  *regex,
                   const char  *input,
                   char        *arg_buf)
{
    bru_argv_out[0] = __FILE__;
    bru_argv_out[1] = "match";

    // clang-format off
    // The last three slots are reserved for '--', regex, and input
    #define N_RESERVED_SLOTS 3
    // clang-format on

    if (bru_argc > MIN_BRU_ARGC) {
        bru_argv_out[2] = strtok(arg_buf, " ");
        for (int i = 3; i < bru_argc - N_RESERVED_SLOTS; i++)
            bru_argv_out[i] = strtok(NULL, " ");
    }

    bru_argv_out[bru_argc - N_RESERVED_SLOTS]     = "--";
    bru_argv_out[bru_argc - N_RESERVED_SLOTS + 1] = regex;
    bru_argv_out[bru_argc - N_RESERVED_SLOTS + 2] = input;

    // clang-format off
    #undef N_RESERVED_SLOTS
    // clang-format on
}

/**
 * Parse a field from a line.
 *
 * @param[in,out] mutable_line The line to parse.
 * @param[in] sep The separator to use.
 * @param[in] replace_special_chars Whether to replace special characters.
 *
 * @return A newly allocated mutable string containing the field.
 */
char *
parse_field(char *mutable_line, const char *sep, int replace_special_chars)

{
    const char *field = strtok(mutable_line, sep);
    assert(field);
    return field_to_char_buf(field, replace_special_chars);
}

void parse_input_fields(const char *line,
                        char      **regex,
                        char      **input,
                        char      **other_args,
                        char      **expected_outcome,
                        char      **expected_match_info)
{
    char *mutable_line = malloc((strlen(line) + 1) * sizeof(char));
    strcpy(mutable_line, line);
    *regex            = parse_field(mutable_line, "\t", REPLACE_SPECIAL_CHARS);
    *input            = parse_field(NULL, "\t", REPLACE_SPECIAL_CHARS);
    *other_args       = parse_field(NULL, "\t", DO_NOT_REPLACE_SPECIAL_CHARS);
    *expected_outcome = parse_field(NULL, "\t", DO_NOT_REPLACE_SPECIAL_CHARS);
    *expected_match_info =
        parse_field(NULL, "\t", DO_NOT_REPLACE_SPECIAL_CHARS);
    free(mutable_line);
}

char *strip(char *s, size_t *stripped_s_len)
{
    assert(s);
    size_t slen  = strlen(s);
    char  *first = s, *last = s + slen;
    if (slen == 0) {
        // Nothing to strip
        if (stripped_s_len) *stripped_s_len = 0;
        return s;
    }
    // Find the last non-whitespace character
    while (last > first && (*last == ' ' || *last == '\t')) last--;
    if (*last == ' ' || *last == '\t')
        *last = '\0';
    else if (last + 1 < first + slen)
        *(last + 1) = '\0';
    // Find the first non-whitespace character

    while (*first == ' ' || *first == '\t') first++;
    if (stripped_s_len) *stripped_s_len = last + 1 - first;
    return first;
}

size_t count(const char c, const char *s)
{
    assert(s);
    size_t count = 0;
    for (const char *p = s; *p; p++)
        if (*p == c) count++;
    return count;
}

void esprint(const char *s)
{
    assert(s);
    for (const char *p = s; *p; p++)
        if (*p == '\n')
            printf("\\n");
        else if (*p == '\t')
            printf("\\t");
        else
            putchar(*p);
}

TestResult find_match(BruOptions *options, BruSRVMMatch **match_found)
{
    TestResult        result         = COMPILATION_FAILURE;
    BruParser        *parser         = NULL;
    BruProgram       *prog           = NULL;
    BruThreadManager *thread_manager = NULL;
    BruSRVM          *srvm           = NULL;

    parser = bru_cli_make_parser(options);
    prog   = bru_cli_make_program(options, parser);
    if (!prog) goto test_done;

    thread_manager = bru_cli_make_thread_manager(options, prog);
    if (!thread_manager) goto test_done;

    srvm = bru_srvm_new(thread_manager, prog);

    if (!srvm) goto test_done;

    *match_found = bru_srvm_match(srvm, options->match.text);
    result       = *match_found ? MATCH : NO_MATCH;
test_done:
    // bru_srvm_free frees the thread manager
    if (srvm) bru_srvm_free(srvm);
    if (prog) bru_program_free(prog);
    if (parser) bru_parser_free(parser);
    return result;
}

TestResult test_result(const char *s)
{
    assert(s);
    for (size_t i = 0; i < sizeof(test_result_str) / sizeof(test_result_str[0]);
         i++)
        if (strcmp(s, (test_result_str[i])) == 0) return i;
    assert((printf("Test result string: '%s'\n", s),
            s = "Only valid test result strings should be passed", 0));
}

long snprintf_match_info(char               *buf,
                         long                buf_len,
                         const BruSRVMMatch *match,
                         const char         *text)
{
    assert(match);
    char *p = buf;
    for (int i = 0; i < match->ncaptures; i++) {
        if (!match->captures[i].str) {
            p += snprintf(p, max(buf_len - (p - buf), 0), "-1:-1");
        } else {
            p += snprintf(p, max(buf_len - (p - buf), 0), "%ld:%ld",
                          match->captures[i].str - text,
                          match->captures[i].str - text +
                              match->captures[i].len);
        }
        if (i < match->ncaptures - 1)
            p += snprintf(p, max(buf_len - (p - buf), 0), " ");
    }
    return (long) p - (long) buf;
}

char *match_info_field(const BruSRVMMatch *match, const char *text)
{
    if (!match) {
        char *empty_field = malloc((strlen(EMPTY_FIELD) + 1) * sizeof(char));
        strcpy(empty_field, EMPTY_FIELD);
        return empty_field;
    }
    int   n          = snprintf_match_info(NULL, 0, match, text);
    char *match_info = malloc((n + 1) * sizeof(char));
    int   n_written  = snprintf_match_info(match_info, n + 1, match, text);
    assert(n_written == n);
    return match_info;
}
