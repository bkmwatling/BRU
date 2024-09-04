#ifndef BRU_RE_PARSER_H
#define BRU_RE_PARSER_H

#include "sre.h"

typedef struct {
    int only_counters;              /**< convert *, +, and ? to counters      */
    int unbounded_counters;         /**< allow counters to be unbounded       */
    int expand_counters;            /**< expand counters                      */
    int whole_match_capture;        /**< save entire match into capture 0     */
    int log_unsupported;            /**< log unsupported features in the expr */
    int allow_repeated_nullability; /**< allow expressions like (a?)*         */
    FILE *logfile;                  /**< the file for logging                 */
} BruParserOpts;

typedef struct {
    const char   *regex; /**< the regex string to parse                       */
    BruParserOpts opts;  /**< the options for parsing                         */
} BruParser;

typedef enum {
    BRU_PARSE_SUCCESS,
    BRU_PARSE_UNSUPPORTED,
    BRU_PARSE_NO_MATCH,
    // errors
    BRU_PARSE_UNMATCHED_PAREN,
    BRU_PARSE_UNQUANTIFIABLE,
    BRU_PARSE_INCOMPLETE_GROUP_STRUCTURE,
    BRU_PARSE_INVALID_ESCAPE,
    BRU_PARSE_MISSING_CLOSING_BRACKET,
    BRU_PARSE_CC_RANGE_OUT_OF_ORDER,
    BRU_PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ,
    BRU_PARSE_NON_EXISTENT_REF,
    BRU_PARSE_END_OF_STRING,
    BRU_PARSE_REPEATED_NULLABILITY,
} BruParseResultCode;

typedef struct {
    BruParseResultCode code; /**< the result code for this result             */
    const char *ch; /**< the pointer into the regex that caused this result   */
} BruParseResult;

#if !defined(BRU_RE_PARSER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_PARSER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&       \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||    \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef BruParserOpts  ParserOpts;
typedef BruParser      Parser;
typedef BruParseResult ParseResult;

typedef BruParseResultCode ParseResultCode;
#    define PARSE_SUCCESS         BRU_PARSE_SUCCESS
#    define PARSE_UNSUPPORTED     BRU_PARSE_UNSUPPORTED
#    define PARSE_NO_MATCH        BRU_PARSE_NO_MATCH
#    define PARSE_UNMATCHED_PAREN BRU_PARSE_UNMATCHED_PAREN
#    define PARSE_UNQUANTIFIABLE  BRU_PARSE_UNQUANTIFIABLE
#    define PARSE_INCOMPLETE_GROUP_STRUCTURE \
        BRU_PARSE_INCOMPLETE_GROUP_STRUCTURE
#    define PARSE_INVALID_ESCAPE          BRU_PARSE_INVALID_ESCAPE
#    define PARSE_MISSING_CLOSING_BRACKET BRU_PARSE_MISSING_CLOSING_BRACKET
#    define PARSE_CC_RANGE_OUT_OF_ORDER   BRU_PARSE_CC_RANGE_OUT_OF_ORDER
#    define PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ \
        BRU_PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ
#    define PARSE_NON_EXISTENT_REF     BRU_PARSE_NON_EXISTENT_REF
#    define PARSE_END_OF_STRING        BRU_PARSE_END_OF_STRING
#    define PARSE_REPEATED_NULLABILITY BRU_PARSE_REPEATED_NULLABILITY

#    define parser_new     bru_parser_new;
#    define parser_default bru_parser_default;
#    define parser_free    bru_parser_free;
#    define parser_parse   bru_parser_parse;
#endif /* BRU_RE_PARSER_ENABLE_SHORT_NAMES */

/**
 * Construct a parser from a regex string with specified options.
 *
 * @param[in] regex the regex string
 * @param[in] opts  the options for the parser
 *
 * @return the constructed parser
 */
BruParser *bru_parser_new(const char *regex, BruParserOpts opts);

/**
 * Construct a parser from a regex string with default options.
 *
 * @param[in] regex the regex string
 *
 * @return the constructed parser
 */
BruParser *bru_parser_default(const char *regex);

/**
 * Free the memory allocated for the parser (does not free the regex string).
 *
 * @param[in] self the parser to free
 */
void bru_parser_free(BruParser *self);

/**
 * Parse the regex stored in the parser into a regex abstract tree.
 *
 * @param[in]  self the parser to parse
 * @param[out] re   the parsed regex tree with the regex string
 *
 * @return the result of the parsing
 */
BruParseResult bru_parser_parse(const BruParser *self, BruRegex *re);

#endif /* BRU_RE_PARSER_H */
