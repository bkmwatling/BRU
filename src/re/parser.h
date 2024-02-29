#ifndef PARSER_H
#define PARSER_H

#include "sre.h"

typedef struct {
    int only_counters;       /**< whether to convert *, +, and ? to counters  */
    int unbounded_counters;  /**< whether to allow counters to be unbounded   */
    int expand_counters;     /**< whether to exapnd counters                  */
    int whole_match_capture; /**< whether to save entire match into capture 0 */
} ParserOpts;

typedef struct {
    const char *regex; /**< the regex string to parse                         */
    ParserOpts  opts;  /**< the options for parsing                           */
} Parser;

typedef enum {
    PARSE_SUCCESS,
    PARSE_NO_MATCH,
    PARSE_UNMATCHED_PAREN,
    PARSE_UNQUANTIFIABLE,
    PARSE_INCOMPLETE_GROUP_STRUCTURE,
    PARSE_INVALID_ESCAPE,
    PARSE_MISSING_CLOSING_BRACKET,
    PARSE_CC_RANGE_OUT_OF_ORDER,
    PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ,
    PARSE_NON_EXISTENT_REF,
    PARSE_END_OF_STRING,
    PARSE_UNSUPPORTED_BACKREF,
    PARSE_UNSUPPORTED_LOOKAHEAD,
    PARSE_UNSUPPORTED_OCTAL,
    PARSE_UNSUPPORTED_HEX,
    PARSE_UNSUPPORTED_UNICODE,
    PARSE_UNSUPPORTED_POSSESSIVE,
    PARSE_UNSUPPORTED_CONTROL_VERB,
    PARSE_UNSUPPORTED_FLAGS,
    PARSE_UNSUPPORTED_CONTROL_CODE,
    PARSE_UNSUPPORTED_LOOKBEHIND,
    PARSE_UNSUPPORTED_NAMED_GROUP,
    PARSE_UNSUPPORTED_RELATIVE_GROUP,
    PARSE_UNSUPPORTED_ATOMIC_GROUP,
    PARSE_UNSUPPORTED_PATTERN_RECURSION,
    PARSE_UNSUPPORTED_LOOKAHEAD_CONDITIONAL,
    PARSE_UNSUPPORTED_CALLOUT,
    PARSE_UNSUPPORTED_GROUP_RESET,
    PARSE_UNSUPPORTED_GROUP_RECURSION,
    PARSE_UNSUPPORTED_WORD_BOUNDARY,
    PARSE_UNSUPPORTED_START_BOUNDARY,
    PARSE_UNSUPPORTED_END_BOUNDARY,
    PARSE_UNSUPPORTED_FIRST_MATCH_BOUNDARY,
    PARSE_UNSUPPORTED_RESET_MATCH_START,
    PARSE_UNSUPPORTED_QUOTING,
    PARSE_UNSUPPORTED_UNICODE_PROPERTY,
    PARSE_UNSUPPORTED_NEWLINE_SEQUENCE,
} ParseResultCode;

typedef struct {
    ParseResultCode code; /**< the result code for this result                */
    const char *ch; /**< the pointer into the regex that caused this result   */
} ParseResult;

/**
 * Construct a parser from a regex string with specified options.
 *
 * @param[in] regex the regex string
 * @param[in] opts  the options for the parser
 *
 * @return the constructed parser
 */
Parser *parser_new(const char *regex, ParserOpts opts);

/**
 * Construct a parser from a regex string with default options.
 *
 * @param[in] regex the regex string
 *
 * @return the constructed parser
 */
Parser *parser_default(const char *regex);

/**
 * Free the memory allocated for the parser (does not free the regex string).
 *
 * @param[in] self the parser to free
 */
void parser_free(Parser *self);

/**
 * Parse the regex stored in the parser into a regex abstract tree.
 *
 * @param[in]  self the parser to parse
 * @param[out] re   the parsed regex tree with the regex string
 *
 * @return the result of the parsing
 */
ParseResult parser_parse(const Parser *self, Regex *re);

#endif /* PARSER_H */
