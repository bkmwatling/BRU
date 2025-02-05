#ifndef TESTING_UTILS_H
#define TESTING_UTILS_H

#include <bru/cli.h>
#include <stdlib.h>

#define EMPTY_FIELD "\"\"" // Empty field in the TSV file.

// Used when converting fields to and from strings
#define REPLACE_SPECIAL_CHARS        1
// Used when converting fields to and from strings
#define DO_NOT_REPLACE_SPECIAL_CHARS 0

static inline char *str_to_field(const char *s, int replace_special_chars)
{
    size_t slen = strlen(s);
    char  *field;

    if (s[0] == '\0') {
        field = (char *) malloc((strlen(EMPTY_FIELD) + 1) * sizeof(char));
        sprintf(field, EMPTY_FIELD);
        return field;
    }

    field = (char *) malloc((slen + 1) * sizeof(char));
    if (replace_special_chars) {
        for (size_t i = 0; i < slen; i++) {
            switch (s[i]) {
                case '\t': field[i] = 'T'; break;
                case '\n': field[i] = 'N'; break;
                case ' ': field[i] = 'S'; break;
                default: field[i] = s[i];
            }
        }
        field[slen] = '\0';
    } else {
        strcpy(field, s);
    }

    return field;
}

/**
 * Convert a field to a mutable string.
 * 
 * @param[in] s The field to convert.
 * @param[in] replace_special_chars Whether to replace special characters.
 * 
 * @return The newly allocated char * that represents the field.
 */
static inline char *field_to_char_buf(const char *s, int replace_special_chars)
{
    size_t slen = strlen(s);
    char  *buf  = (char *) malloc((slen + 1) * sizeof(char));

    if (strcmp(s, EMPTY_FIELD) == 0) {
        buf[0] = '\0';
        return buf;
    }

    if (replace_special_chars) {
        for (size_t i = 0; i < slen; i++) {
            switch (s[i]) {
                case 'T': buf[i] = '\t'; break;
                case 'N': buf[i] = '\n'; break;
                case 'S': buf[i] = ' '; break;
                default: buf[i] = s[i];
            }
        }
        buf[slen] = '\0';
    } else {
        strcpy(buf, s);
    }

    return buf;
}

typedef enum {
    SETUP_FAILURE = 0,
    COMPILATION_FAILURE,
    MATCHING_ERROR,
    MATCH,
    NO_MATCH,
    TIMEOUT,
} TestResult;

extern const char *test_result_str[];

TestResult test_result(const char *s);

short read_line(char **line_buf, size_t *buf_len, FILE *f, size_t *line_len);

void parse_input_fields(const char  *line,
                        char **regex,
                        char **input,
                        char **other_args,
                        char **expected_outcome,
                        char **match_info);

/**
 * Allocate and initialize memory for the argument buffer to pass to the bru
 * executable.
 *
 * @param[out] bru_argc_out The number of slots in the newly allocated buffer.
 * @param[in] regex The regex to match.
 * @param[in] input The input text to match against.
 * @param[in,out] arg_buf The buffer to use for strtok.
 *
 * @return The newly allocated and initialized buffer.
 */
const char **new_bru_argv(int        *bru_argc_out,
                          const char *regex,
                          const char *input,
                          char       *arg_buf);

/**
 * Free the memory allocated for the argument buffer to pass to the bru.
 *
 * @param[in,out] bru_argv The buffer to free.
 */
void free_bru_argv(const char **bru_argv, char *arg_token_buf);

/**
 * Create the argument buffer to pass to the bru executable.
 *
 * Format of bru_args:
 *  (
 *      [executable_name, subcommand] +
 *      [arg for arg in other_args] +
 *      ['--'] + [regex] + [input]
 *  )
 *
 * @param[in] bru_argc The number of slots in the buffer.
 * @param[out] bru_argv_out The buffer of arguments to initialize.
 * @param[in] regex The regex to match.
 * @param[in] input The input text to match against.
 * @param[in,out] arg_buf Other arguments and/or options to pass to the bru
 * executable. The string will be modified by strtok.
 *
 * @note The caller is responsible for freeing the memory allocated for
 * bru_argv_out.
 */
void init_bru_argv(int    bru_argc,
                   const char **bru_argv_out,
                   const char  *regex,
                   const char  *input,
                   char  *arg_buf);

TestResult find_match(BruOptions *options, BruSRVMMatch **match_found);

/**
 * Strip trailing whitespace and return a pointer to the first non-whitespace
 * character.
 *
 * @note This function does not allocate memory. The pointer returned points
 * somewhere in the buffer `s`.
 *
 * @param[in,out] s The string to strip.
 * @param[out] stripped_len The length of the stripped string.
 *
 * @return A pointer to the first non-whitespace character in s.
 */
char *strip(char *s, size_t *stripped_len);

/**
 * Count the number of occurrences of a character in a string.
 *
 * @param[in] c The character to count.
 * @param[in] s The string to search.
 *
 * @return The number of occurrences of c in s.
 */
size_t count(const char c, const char *s);

/**
 * Print a string with special characters escaped.
 *
 * @param[in] s The string to print.
 */
void esprint(const char *s);

/**
 * Return a newly allocated string that represents the match information in the
 * format "start:end start:end ...".
 *
 * @param[in] match The match to print.
 * @param[in] text The text that was searched.
 */
char *match_info_field(const BruSRVMMatch *match, const char *text);

#endif // TESTING_UTILS_H
