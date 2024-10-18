#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../types.h"
#include "../utils.h"
#include "parser.h"
#include "sre.h"

/* --- Preprocessor directives ---------------------------------------------- */

#define PARSER_OPTS_DEFAULT ((BruParserOpts){ 0, 1, 0, 0, 1, 0, stderr })

#define SET_RID(node, ps) \
    if (node) (node)->rid = (ps)->next_rid++

#define PARSE_RES(code, ch) ((BruParseResult){ (code), (ch) })

#define SUCCEEDED(code) ((code) < BRU_PARSE_NO_MATCH)

#define FAILED(code) ((code) >= BRU_PARSE_NO_MATCH)

#define NOT_MATCHED(code) ((code) == BRU_PARSE_NO_MATCH)

#define ERRORED(code) ((code) > BRU_PARSE_NO_MATCH)

#define DOT_NINTERVALS 2

#define UNICODE_END "\U0010ffff"

#define INTERVAL_LIST_ITEM_INIT(item, intrvl)          \
    do {                                               \
        (item)           = calloc(1, sizeof(*(item))); \
        (item)->interval = (intrvl);                   \
    } while (0)

#define SKIP_UNTIL(pred, res, parse_state)            \
    do {                                              \
        while (!(pred)) {                             \
            if (*(parse_state)->ch == '\0') {         \
                (res).code = BRU_PARSE_END_OF_STRING; \
                break;                                \
            }                                         \
            (parse_state)->ch++;                      \
        }                                             \
    } while (0)

#define FLAG_UNSUPPORTED(code, ps) ((*(ps)->unsupported_feats)[code] = TRUE)

/* --- Type definitions ----------------------------------------------------- */

typedef enum {
    BRU_UNSUPPORTED_BACKREF,
    BRU_UNSUPPORTED_LOOKAHEAD,
    BRU_UNSUPPORTED_OCTAL,
    BRU_UNSUPPORTED_HEX,
    BRU_UNSUPPORTED_UNICODE,
    BRU_UNSUPPORTED_POSSESSIVE,
    BRU_UNSUPPORTED_CONTROL_VERB,
    BRU_UNSUPPORTED_FLAGS,
    BRU_UNSUPPORTED_CONTROL_CODE,
    BRU_UNSUPPORTED_LOOKBEHIND,
    BRU_UNSUPPORTED_NAMED_GROUP,
    BRU_UNSUPPORTED_RELATIVE_GROUP,
    BRU_UNSUPPORTED_ATOMIC_GROUP,
    BRU_UNSUPPORTED_PATTERN_RECURSION,
    BRU_UNSUPPORTED_LOOKAHEAD_CONDITIONAL,
    BRU_UNSUPPORTED_CALLOUT,
    BRU_UNSUPPORTED_GROUP_RESET,
    BRU_UNSUPPORTED_GROUP_RECURSION,
    BRU_UNSUPPORTED_WORD_BOUNDARY,
    BRU_UNSUPPORTED_START_BOUNDARY,
    BRU_UNSUPPORTED_END_BOUNDARY,
    BRU_UNSUPPORTED_FIRST_MATCH_BOUNDARY,
    BRU_UNSUPPORTED_RESET_MATCH_START,
    BRU_UNSUPPORTED_QUOTING,
    BRU_UNSUPPORTED_UNICODE_PROPERTY,
    BRU_UNSUPPORTED_NEWLINE_SEQUENCE,
    BRU_NUM_UNSUPPORTED_CODES
} BruUnsupportedFeatureCode;

typedef struct bru_interval_list_item BruIntervalListItem;

struct bru_interval_list_item {
    BruInterval          interval;
    BruIntervalListItem *prev;
    BruIntervalListItem *next;
};

typedef struct {
    int                  neg;
    size_t               len;
    BruIntervalListItem *sentinel;
} BruIntervalList;

typedef struct {
    BruUnsupportedFeatureCode (*unsupported_feats)[BRU_NUM_UNSUPPORTED_CODES];
    bru_byte_t allow_repeated_nullability; /*<< allow expressions like (a?)*  */
    const char  *ch;
    int          in_group;
    int          in_lookahead;
    bru_len_t    ncaptures;
    bru_regex_id next_rid;
} BruParseState;

/* --- Helper function prototypes ------------------------------------------- */

static BruParseResult parse_alt(const BruParser *self,
                                BruParseState   *ps,
                                BruRegexNode   **re /**< out parameter */);

static BruParseResult parse_expr(const BruParser *self,
                                 BruParseState   *ps,
                                 BruRegexNode   **re /**< out parameter */);

static BruParseResult parse_elem(const BruParser *self,
                                 BruParseState   *ps,
                                 BruRegexNode   **re /**< out parameter */);

static BruParseResult parse_atom(const BruParser *self,
                                 BruParseState   *ps,
                                 BruRegexNode   **re /**< out parameter */);

static BruParseResult
parse_quantifier(const BruParser *self,
                 BruParseState   *ps,
                 BruRegexNode   **re /**< in,out parameter */);

static BruParseResult parse_curly(BruParseState *ps,
                                  bru_cntr_t    *min /**< out parameter */,
                                  bru_cntr_t    *max /**< out parameter */);

static BruParseResult parse_paren(const BruParser *self,
                                  BruParseState   *ps,
                                  BruRegexNode   **re /**< out parameter */);

static BruParseResult parse_cc(BruParseState *ps,
                               BruRegexNode **re /**< out parameter */);

static BruParseResult
parse_cc_atom(BruParseState *ps, BruIntervalList *list /**< out parameter */);

static BruParseResult parse_escape(BruParseState *ps,
                                   BruRegexNode **re /**< out parameter */);

static BruParseResult parse_escape_char(BruParseState *ps,
                                        const char **ch /**< out parameter */);

static BruParseResult
parse_escape_cc(BruParseState *ps, BruIntervalList *list /**< out parameter */);

static BruParseResult
parse_posix_cc(BruParseState *ps, BruIntervalList *list /**< out parameter */);

static BruParseResult skip_comment(BruParseState *ps);

static void find_matching_closing_parenthesis(BruParseState *ps);

static void print_unsupported_feature(unsigned int feature_idx, FILE *stream);

static BruRegexNode *parser_regex_counter(BruRegexNode  *child,
                                          bru_byte_t     greedy,
                                          bru_cntr_t     min,
                                          bru_cntr_t     max,
                                          int            expand_counters,
                                          BruParseState *ps);

/* --- API function definitions --------------------------------------------- */

BruParser *bru_parser_new(const char *regex, BruParserOpts opts)
{
    BruParser *parser = malloc(sizeof(*parser));

    parser->regex = regex;
    parser->opts  = opts;

    return parser;
}

BruParser *bru_parser_default(const char *regex)
{
    return bru_parser_new(regex, PARSER_OPTS_DEFAULT);
}

void bru_parser_free(BruParser *self) { free(self); }

BruParseResult bru_parser_parse(const BruParser *self, BruRegex *re)
{
    BruUnsupportedFeatureCode unsupported_feats[BRU_NUM_UNSUPPORTED_CODES] = {
        0
    };
    BruRegexNode  *r         = NULL;
    bru_len_t      ncaptures = self->opts.whole_match_capture ? 1 : 0;
    BruParseState  ps        = { &unsupported_feats,
                                 self->opts.allow_repeated_nullability,
                                 self->regex,
                                 0,
                                 0,
                                 ncaptures,
                                 0 };
    BruParseResult res       = parse_alt(self, &ps, &r);
    unsigned int   i;

    if (SUCCEEDED(res.code)) {
        if (self->opts.whole_match_capture) {
            r      = bru_regex_capture(r, 0);
            r->rid = ps.next_rid++;
        }

        if (re) *re = (BruRegex){ self->regex, r };
    } else if (r) {
        bru_regex_node_free(r);
    }

    if (self->opts.log_unsupported && res.code == BRU_PARSE_UNSUPPORTED) {
        fprintf(self->opts.logfile,
                "------------ UNSUPPORTED FEATURE CODES ------------\n");
        for (i = 0; i < BRU_NUM_UNSUPPORTED_CODES; i++)
            if (unsupported_feats[i])
                print_unsupported_feature(i, self->opts.logfile);
        fprintf(self->opts.logfile,
                "------------ UNSUPPORTED FEATURE CODES ------------\n");
    }

    return res;
}

/* --- Helper function definitions ------------------------------------------ */

static BruIntervals *dot(void)
{
    BruIntervals *dot = bru_intervals_new(TRUE, DOT_NINTERVALS);

    dot->intervals[0] = bru_interval("\0", "\0");
    dot->intervals[1] = bru_interval("\n", "\n");

    return dot;
}

static BruParseResult parse_alt(const BruParser *self,
                                BruParseState   *ps,
                                BruRegexNode   **re /**< out parameter */)
{
    BruRegexNode  *r;
    BruParseResult res, prev_res;

    res = parse_expr(self, ps, re);
    if (FAILED(res.code)) return res;
    while (*ps->ch == '|') {
        ps->ch++;
        r        = NULL;
        prev_res = res;
        res      = parse_expr(self, ps, &r);
        // NOTE: prev_res must be SUCCESS-ish, hence, this will never
        // overwrite a failure code in res.
        if (prev_res.code > res.code) res.code = prev_res.code;
        if (FAILED(res.code)) {
            if (r) bru_regex_node_free(r);
            return res;
        }
        *re = bru_regex_branch(BRU_ALT, *re, r);
        SET_RID(*re, ps);
    }

    return res;
}

static BruParseResult parse_expr(const BruParser *self,
                                 BruParseState   *ps,
                                 BruRegexNode   **re /**< out parameter */)
{
    BruRegexNode  *r;
    BruParseResult res, prev_res;

    res = parse_elem(self, ps, re);
    if (NOT_MATCHED(res.code)) {
        *re = bru_regex_new(BRU_EPSILON);
        SET_RID(*re, ps);
        return PARSE_RES(BRU_PARSE_SUCCESS, ps->ch);
    } else if (ERRORED(res.code)) {
        return res;
    }
    while (*ps->ch) {
        r        = NULL;
        prev_res = res;
        res      = parse_elem(self, ps, &r);

        // NOTE: prev_res must be SUCCESS-ish, hence, this will never
        // overwrite a failure code in res.
        if (NOT_MATCHED(res.code)) {
            if (r) bru_regex_node_free(r);
            res.code = prev_res.code;
            break;
        } else if (ERRORED(res.code)) {
            if (r) bru_regex_node_free(r);
            break;
        } else if (prev_res.code > res.code)
            res.code = prev_res.code;

        *re = bru_regex_branch(BRU_CONCAT, *re, r);
        SET_RID(*re, ps);
    }

    return res;
}

static BruParseResult parse_elem(const BruParser *self,
                                 BruParseState   *ps,
                                 BruRegexNode   **re /**< out parameter */)
{
    BruParseResult     res;
    BruParseResultCode prev_code;

    res = parse_atom(self, ps, re);
    if (FAILED(res.code)) return res;
    prev_code = res.code;
    res       = parse_quantifier(self, ps, re);
    if (NOT_MATCHED(res.code) || (SUCCEEDED(res.code) && prev_code > res.code))
        res.code = prev_code;

    return res;
}

static BruParseResult parse_atom(const BruParser *self,
                                 BruParseState   *ps,
                                 BruRegexNode   **re /**< out parameter */)
{
    BruParseResult     res;
    BruParseResultCode prev_code;

    res = skip_comment(ps);
    if (ERRORED(res.code)) return res;

    switch (*ps->ch) {
        case '\\': res = parse_escape(ps, re); break;
        case '(': res = parse_paren(self, ps, re); break;
        case '[': res = parse_cc(ps, re); break;

        case '|': res = PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch); break;

        case '^':
            *re = bru_regex_new(BRU_CARET);
            SET_RID(*re, ps);
            res = PARSE_RES(BRU_PARSE_SUCCESS, ps->ch++);
            break;

        case '$':
            *re = bru_regex_new(BRU_DOLLAR);
            SET_RID(*re, ps);
            res = PARSE_RES(BRU_PARSE_SUCCESS, ps->ch++);
            break;

        // case '#':
        //     *re = regex_new(BRU_MEMOISE);
        //     SET_RID(*re, ps);
        //     res = PARSE_RES(PARSE_SUCCESS, ps->ch++);
        //     break;
        //
        case '.':
            *re = bru_regex_cc(dot());
            SET_RID(*re, ps);
            res = PARSE_RES(BRU_PARSE_SUCCESS, ps->ch++);
            break;

        case '\0':
            // *re = regex_new(EPSILON);
            // SET_RID(*re, ps);
            res = PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);
            break;

        case ')':
            if (ps->in_group)
                res = PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);
            else
                res = PARSE_RES(BRU_PARSE_UNMATCHED_PAREN, ps->ch);
            break;

        case '*': /* fallthrough */
        case '+': /* fallthrough */
        case '?': res = PARSE_RES(BRU_PARSE_UNQUANTIFIABLE, ps->ch); break;

        case '{':
            res = parse_curly(ps, NULL, NULL);
            if (SUCCEEDED(res.code)) {
                res.code = BRU_PARSE_UNQUANTIFIABLE;
                break;
            }
            /* fallthrough */

        default:
            *re = bru_regex_literal(ps->ch);
            SET_RID(*re, ps);
            res = PARSE_RES(BRU_PARSE_SUCCESS, ps->ch++);
            break;
    }

    if (SUCCEEDED(res.code)) {
        prev_code = res.code;
        res       = skip_comment(ps);
        if (SUCCEEDED(res.code)) res.code = prev_code;
    }

    return res;
}

static BruParseResult
parse_quantifier(const BruParser *self,
                 BruParseState   *ps,
                 BruRegexNode   **re /**< in,out parameter */)
{
    BruRegexNode  *tmp;
    BruParseResult res = { BRU_PARSE_SUCCESS, NULL };
    bru_byte_t     greedy;
    bru_cntr_t     min, max;

    /* check for quantifier */
    switch (*ps->ch) {
        case '*':
            min = 0;
            max = BRU_CNTR_MAX;
            ps->ch++;
            break;
        case '+':
            min = 1;
            max = BRU_CNTR_MAX;
            ps->ch++;
            break;
        case '?':
            min = 0;
            max = 1;
            ps->ch++;
            break;
        case '{':
            res = parse_curly(ps, &min, &max);
            if (FAILED(res.code)) return res;
            break;

        default: return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);
    }

    /* check that child is quantifiable */
    switch ((*re)->type) {
        case BRU_EPSILON: /* fallthrough */
        case BRU_CARET:   /* fallthrough */
        /* case MEMOISE: */
        case BRU_DOLLAR: return PARSE_RES(BRU_PARSE_UNQUANTIFIABLE, ps->ch);

        case BRU_LITERAL:   /* fallthrough */
        case BRU_CC:        /* fallthrough */
        case BRU_ALT:       /* fallthrough */
        case BRU_CONCAT:    /* fallthrough */
        case BRU_CAPTURE:   /* fallthrough */
        case BRU_STAR:      /* fallthrough */
        case BRU_PLUS:      /* fallthrough */
        case BRU_QUES:      /* fallthrough */
        case BRU_COUNTER:   /* fallthrough */
        case BRU_LOOKAHEAD: /* fallthrough */
        case BRU_BACKREFERENCE:
            if (!ps->allow_repeated_nullability && max == BRU_CNTR_MAX &&
                (*re)->nullable) {
                return PARSE_RES(BRU_PARSE_REPEATED_NULLABILITY, ps->ch);
            }
            break;

        case BRU_NREGEXTYPES: assert(0 && "unreachable");
    }

    /* check for lazy */
    switch (*ps->ch) {
        case '?':
            greedy = FALSE;
            ps->ch++;
            break;

        /* NOTE: unsupported */
        case '+':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_POSSESSIVE, ps);
            ps->ch++;
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        default: greedy = TRUE;
    }

    /* check for single or zero repitition */
    if (min == 1 && max == 1) {
        res.ch = ps->ch;
        return res;
    }
    if (min == 0 && max == 0) {
        bru_regex_node_free(*re);
        *re = bru_regex_new(BRU_EPSILON);
        SET_RID(*re, ps);
        res.ch = ps->ch;
        return res;
    }

    /* apply quantifier */
    if (self->opts.only_counters) {
        *re = bru_regex_counter(*re, greedy, min, max);
        SET_RID(*re, ps);
    } else if (min == 0 && max == BRU_CNTR_MAX) {
        *re = bru_regex_repetition(BRU_STAR, *re, greedy);
        SET_RID(*re, ps);
    } else if (min == 1 && max == BRU_CNTR_MAX) {
        *re = bru_regex_repetition(BRU_PLUS, *re, greedy);
        SET_RID(*re, ps);
    } else if (min == 0 && max == 1) {
        *re = bru_regex_repetition(BRU_QUES, *re, greedy);
        SET_RID(*re, ps);
    } else if (self->opts.unbounded_counters || max < BRU_CNTR_MAX) {
        *re = parser_regex_counter(*re, greedy, min, max,
                                   self->opts.expand_counters, ps);
    } else {
        tmp = bru_regex_repetition(BRU_STAR, *re, greedy);
        *re = bru_regex_branch(
            BRU_CONCAT,
            parser_regex_counter(bru_regex_clone(*re), greedy, min, min,
                                 self->opts.expand_counters, ps),
            tmp);
        SET_RID(tmp, ps);
        SET_RID(*re, ps);
    }

    res.ch = ps->ch;
    return res;
}

static BruParseResult parse_curly(BruParseState *ps,
                                  bru_cntr_t    *min /**< out parameter */,
                                  bru_cntr_t    *max /**< out parameter */)
{
    char        ch;
    bru_cntr_t  m, n;
    const char *next_ch = ps->ch;

    if (*next_ch++ != '{' || !isdigit(*next_ch))
        return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);

    m = 0;
    while ((ch = *next_ch++) != ',') {
        if (isdigit(ch)) {
            m = m * 10 + (ch - '0');
        } else if (ch == '}') {
            n = m;
            goto done;
        } else {
            return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);
        }
    }

    n = *next_ch == '}' ? BRU_CNTR_MAX : 0;
    while ((ch = *next_ch++) != '}')
        if (isdigit(ch))
            n = n * 10 + (ch - '0');
        else
            return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);

done:
    ps->ch = next_ch;
    if (min) *min = m;
    if (max) *max = n;
    return PARSE_RES(BRU_PARSE_SUCCESS, ps->ch);
}

static BruParseResult parse_paren(const BruParser *self,
                                  BruParseState   *ps,
                                  BruRegexNode   **re /**< out parameter */)
{
    BruParseResult            res;
    BruParseState             ps_tmp;
    BruUnsupportedFeatureCode unsupported_code;
    const char               *ch;
    bru_len_t                 ncaptures;
    int                       is_lookahead = FALSE, pos = FALSE;

    if (*(ch = ps->ch) != '(') return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);
    ps->ch++;

    switch (*ps->ch) {
        /* control verbs */
        case '*':
            unsupported_code = BRU_UNSUPPORTED_CONTROL_VERB;
            goto unsupported_group;
            break;

        case '?':
            switch (*++ps->ch) {
                /* lookbehind */
                case '<':
                    unsupported_code = BRU_UNSUPPORTED_LOOKBEHIND;
                    goto unsupported_group;

                /* named groups */
                case 'P': /* fallthrough */
                case '\'':
                    unsupported_code = BRU_UNSUPPORTED_NAMED_GROUP;
                    goto unsupported_group;

                /* relative group back ref */
                case '-': /* fallthrough */
                case '+':
                    unsupported_code = BRU_UNSUPPORTED_RELATIVE_GROUP;
                    goto unsupported_group;

                /* atomic grouping */
                case '>':
                    unsupported_code = BRU_UNSUPPORTED_ATOMIC_GROUP;
                    goto unsupported_group;

                /* pattern recursion */
                case 'R':
                    unsupported_code = BRU_UNSUPPORTED_PATTERN_RECURSION;
                    goto unsupported_group;

                /* conditional lookahead */
                case '(':
                    unsupported_code = BRU_UNSUPPORTED_LOOKAHEAD_CONDITIONAL;
                    goto unsupported_group;

                /* callout */
                case 'C':
                    unsupported_code = BRU_UNSUPPORTED_CALLOUT;
                    goto unsupported_group;

                // TODO: support lookaheads
                case '=': // pos = TRUE; /* fallthrough */
                case '!':
                    // is_lookahead = TRUE; break;
                    unsupported_code = BRU_UNSUPPORTED_LOOKAHEAD;
                    goto unsupported_group;

                /* reset group numbers in alternations */
                case '|':
                    unsupported_code = BRU_UNSUPPORTED_GROUP_RESET;
                    goto unsupported_group;

                /* flags */
                case 'i': /* fallthrough */
                case 'J': /* fallthrough */
                case 'm': /* fallthrough */
                case 's': /* fallthrough */
                case 'U': /* fallthrough */
                case 'x':
                    unsupported_code = BRU_UNSUPPORTED_FLAGS;
                    goto unsupported_group;

                /* non-capturing */
                case ':': break;

                /* group recursion */
                default:
                    if (!isdigit(*ps->ch))
                        return PARSE_RES(BRU_PARSE_INCOMPLETE_GROUP_STRUCTURE,
                                         ch);

                    unsupported_code = BRU_UNSUPPORTED_GROUP_RECURSION;
                    goto unsupported_group;
            }
            ps->ch++;
            ps_tmp        = (BruParseState){ ps->unsupported_feats,
                                             ps->allow_repeated_nullability,
                                             ps->ch,
                                             TRUE,
                                             ps->in_lookahead || is_lookahead,
                                             ps->ncaptures,
                                             ps->next_rid };
            res           = parse_alt(self, &ps_tmp, re);
            ps->ch        = ps_tmp.ch;
            ps->ncaptures = ps_tmp.ncaptures;
            ps->next_rid  = ps_tmp.next_rid;
            if (FAILED(res.code)) return res;
            if (*ps->ch != ')')
                return PARSE_RES(BRU_PARSE_INCOMPLETE_GROUP_STRUCTURE, ch);

            if (is_lookahead) {
                *re = bru_regex_lookahead(*re, pos);
                SET_RID(*re, ps);
            }
            break;

        /* capture group */
        default:
            if (!ps->in_lookahead) ncaptures = ps->ncaptures++;
            ps_tmp        = (BruParseState){ ps->unsupported_feats,
                                             ps->allow_repeated_nullability,
                                             ps->ch,
                                             TRUE,
                                             ps->in_lookahead,
                                             ps->ncaptures,
                                             ps->next_rid };
            res           = parse_alt(self, &ps_tmp, re);
            ps->ch        = ps_tmp.ch;
            ps->ncaptures = ps_tmp.ncaptures;
            ps->next_rid  = ps_tmp.next_rid;
            if (FAILED(res.code)) return res;
            if (*ps->ch != ')')
                return PARSE_RES(BRU_PARSE_INCOMPLETE_GROUP_STRUCTURE, ch);

            if (!ps->in_lookahead) {
                *re = bru_regex_capture(*re, ncaptures);
                SET_RID(*re, ps);
            }
            break;
    }

done:
    ps->ch++;
    return res;

unsupported_group:
    FLAG_UNSUPPORTED(unsupported_code, ps);
    find_matching_closing_parenthesis(ps);
    if (*ps->ch != ')')
        return PARSE_RES(BRU_PARSE_INCOMPLETE_GROUP_STRUCTURE, ch);
    *re = bru_regex_new(BRU_EPSILON);
    SET_RID(*re, ps);
    res = PARSE_RES(BRU_PARSE_UNSUPPORTED, ps->ch);
    goto done;
}

static BruParseResult parse_cc(BruParseState *ps,
                               BruRegexNode **re /**< out parameter */)
{
    BruParseResult       res;
    BruIntervalList      list = { 0 };
    BruIntervalListItem *item, *next;
    BruIntervals        *intervals;
    const char          *ch;
    size_t               i;
    int                  neg = FALSE;

    if (*(ch = ps->ch) != '[') return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);

    if (*++ps->ch == '^') {
        neg = TRUE;
        ps->ch++;
    }

    BRU_DLL_INIT(list.sentinel);
    do {
        res = parse_cc_atom(ps, &list);
        if (ERRORED(res.code)) {
            if (res.code == BRU_PARSE_MISSING_CLOSING_BRACKET) res.ch = ch;
            goto done;
        }
    } while (*ps->ch != ']');
    ps->ch++;

    intervals = bru_intervals_new(neg, list.len);
    for (i = 0, item = list.sentinel->next; item != list.sentinel;
         i++, item   = item->next) {
        intervals->intervals[i] = item->interval;
    }

    *re = bru_regex_cc(intervals);
    SET_RID(*re, ps);

done:
    BRU_DLL_FREE(list.sentinel, free, item, next);
    return res;
}

static BruParseResult parse_cc_atom(BruParseState   *ps,
                                    BruIntervalList *list /**< out parameter */)
{
    BruParseResult       res;
    BruIntervalListItem *item;
    const char          *ch, *ch_start = NULL, *ch_end = NULL;

    switch (*ps->ch) {
        case '[':
            res = parse_posix_cc(ps, list);
            if (NOT_MATCHED(res.code)) {
                ch_start = "[";
                res.code = BRU_PARSE_SUCCESS;
                ps->ch++;
            }
            break;

        case '\\':
            res = parse_escape_char(ps, &ch_start);
            if (NOT_MATCHED(res.code)) {
                res = parse_escape_cc(ps, list);
                if (NOT_MATCHED(res.code))
                    res = PARSE_RES(BRU_PARSE_INVALID_ESCAPE, ps->ch);
            }
            break;

        case '\0':
            res = PARSE_RES(BRU_PARSE_MISSING_CLOSING_BRACKET, ps->ch);
            break;

        default: /* NOTE: should handle ']' at beginning of character class */
            ch_start = stc_utf8_str_advance(&ps->ch);
            res      = PARSE_RES(BRU_PARSE_SUCCESS, ps->ch);
            break;
    }

    if (ERRORED(res.code)) return res;

    if (*(ch = ps->ch) != '-') {
        if (ch_start) {
            INTERVAL_LIST_ITEM_INIT(item, bru_interval(ch_start, ch_start));
            BRU_DLL_PUSH_BACK(list->sentinel, item);
            list->len++;
        }
        return res;
    }

    if (!ch_start && ps->ch[1] != ']')
        return PARSE_RES(BRU_PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ, ch);

    switch (*++ps->ch) {
        case '[':
            res = parse_posix_cc(ps, list);
            if (NOT_MATCHED(res.code)) {
                ch_end   = "[";
                res.code = BRU_PARSE_SUCCESS;
                ps->ch++;
            } else if (SUCCEEDED(res.code)) {
                res = PARSE_RES(BRU_PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ,
                                ch);
            }
            break;

        case '\\':
            res = parse_escape_char(ps, &ch_end);
            if (NOT_MATCHED(res.code)) {
                res = parse_escape_cc(ps, list);
                if (NOT_MATCHED(res.code))
                    res = PARSE_RES(BRU_PARSE_INVALID_ESCAPE, ps->ch);
                else if (SUCCEEDED(res.code))
                    res = PARSE_RES(
                        BRU_PARSE_CC_RANGE_CONTAINS_SHORTHAND_ESC_SEQ, ch);
            }
            break;

        case ']':
            if (ch_start) {
                INTERVAL_LIST_ITEM_INIT(item, bru_interval(ch_start, ch_start));
                BRU_DLL_PUSH_BACK(list->sentinel, item);
                list->len++;
            }
            INTERVAL_LIST_ITEM_INIT(item, bru_interval("-", "-"));
            BRU_DLL_PUSH_BACK(list->sentinel, item);
            list->len++;
            res = PARSE_RES(BRU_PARSE_SUCCESS, ps->ch);
            break;

        case '\0':
            res = PARSE_RES(BRU_PARSE_MISSING_CLOSING_BRACKET, ps->ch);
            break;

        default:
            ch_end = stc_utf8_str_advance(&ps->ch);
            res    = PARSE_RES(BRU_PARSE_SUCCESS, ps->ch);
            break;
    }

    if (ERRORED(res.code) || !ch_end) return res;

    if (stc_utf8_cmp(ch_start, ch_end) <= 0) {
        INTERVAL_LIST_ITEM_INIT(item, bru_interval(ch_start, ch_end));
        BRU_DLL_PUSH_BACK(list->sentinel, item);
        list->len++;
    } else {
        res = PARSE_RES(BRU_PARSE_CC_RANGE_OUT_OF_ORDER, ch);
    }

    return res;
}

static BruParseResult parse_escape(BruParseState *ps,
                                   BruRegexNode **re /**< out parameter */)
{
    BruParseResult       res;
    BruIntervalList      list = { 0 };
    BruIntervalListItem *item, *next;
    BruIntervals        *intervals;
    const char          *ch;
    size_t               i;
    // TODO: use for backreferences
    // len_t             k;

    if (*ps->ch != '\\') return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);

    res = parse_escape_char(ps, &ch);
    if (ERRORED(res.code)) return res;
    if (SUCCEEDED(res.code)) {
        *re = bru_regex_literal(ch);
        SET_RID(*re, ps);
        return res;
    }

    BRU_DLL_INIT(list.sentinel);
    res = parse_escape_cc(ps, &list);
    if (ERRORED(res.code)) goto done;
    if (SUCCEEDED(res.code)) {
        intervals = bru_intervals_new(FALSE, list.len);
        for (i = 0, item = list.sentinel->next; item != list.sentinel;
             i++, item   = item->next) {
            intervals->intervals[i] = item->interval;
        }

        *re = bru_regex_cc(intervals);
        SET_RID(*re, ps);
        goto done;
    }

    res.code = BRU_PARSE_SUCCESS;
    ch       = ps->ch++;
    switch (*ps->ch) {
        case 'B': /* fallthrough */
        case 'b':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_WORD_BOUNDARY, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'A':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_START_BOUNDARY, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'z': /* fallthrough */
        case 'Z':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_END_BOUNDARY, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'G':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_FIRST_MATCH_BOUNDARY, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'g':
            ps->ch++;
            if (*ps->ch == '{') {
                SKIP_UNTIL(*ps->ch == '}', res, ps);
                if (FAILED(res.code)) return res;
            } else {
                SKIP_UNTIL(!isdigit(*ps->ch), res, ps);
                if (SUCCEEDED(res.code)) ps->ch--;
            }
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_BACKREF, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'k':
            switch (ps->ch[1]) {
                case '<': SKIP_UNTIL(*ps->ch == '>', res, ps); break;
                case '\\': SKIP_UNTIL(*ps->ch == '\\', res, ps); break;
                case '{': SKIP_UNTIL(*ps->ch == '}', res, ps); break;
                default: break;
            }
            if (FAILED(res.code)) return res;
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_BACKREF, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;
        case 'K':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_RESET_MATCH_START, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        // technically will stop at '\\E' as well, but since the semantics of
        // \Q ... \E is to treat characters as is, this is fine (i.e. the second
        // \ is not escaped)
        case 'Q':
            SKIP_UNTIL(*ps->ch == 'E' && ps->ch[-1] == '\\', res, ps);
            if (FAILED(res.code)) return res; /* fallthrough */
        case 'E': // NOTE: \E only has special meaning if \Q was already seen
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_QUOTING, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'p': /* fallthrough */
        case 'P':
            ps->ch++;
            if (*ps->ch == '{') {
                SKIP_UNTIL(*ps->ch == '}', res, ps);
                if (FAILED(res.code)) return res;
            }
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_UNICODE_PROPERTY, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'R':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_NEWLINE_SEQUENCE, ps);
            *re = bru_regex_new(BRU_EPSILON);
            SET_RID(*re, ps);
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        default:
            if (isdigit(*ps->ch)) {
                // TODO: support backreferences
                // k = *ps->ch - '0';
                // if (k < ps->ncaptures) {
                //     *re = regex_backreference(k);
                //     SET_RID(*re, ps);
                // } else {
                //     res.code = PARSE_NON_EXISTENT_REF;
                // }
                FLAG_UNSUPPORTED(BRU_UNSUPPORTED_BACKREF, ps);
                *re = bru_regex_new(BRU_EPSILON);
                SET_RID(*re, ps);
                res.code = BRU_PARSE_UNSUPPORTED;
                break;
            } else {
                res.code = BRU_PARSE_INVALID_ESCAPE;
            }
            break;
    }

    if (SUCCEEDED(res.code)) ch = ++ps->ch;
    res.ch = ch;

done:
    BRU_DLL_FREE(list.sentinel, free, item, next);
    return res;
}

static BruParseResult parse_escape_char(BruParseState *ps,
                                        const char   **ch /**< out parameter */)
{
    BruParseResult res = { BRU_PARSE_SUCCESS, NULL };
    const char    *next_ch;

    if (*(next_ch = ps->ch) != '\\')
        return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);

    switch (*++next_ch) {
        case 't': *ch = "\t"; break;
        case 'f': *ch = "\f"; break;
        case 'n': *ch = "\n"; break;
        case 'r': *ch = "\r"; break;
        case 'v': *ch = "\v"; break;

        case 'a': /* fallthrough */
        case 'e': *ch = "\7"; break;

        case 'c':
            /* XXX: as I understand it is a simple offset on only ascii:
             * @  : 0
             * A-Z: *ps->ch - 'A' + 1
             * a-z: *ps->ch - 'a' + 1
             *  -?: *ps->ch + 64
             * [-`: *ps->ch - 64
             * {-~: *ps->ch - 64 */
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_CONTROL_CODE, ps);
            *ch      = "";
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'o':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_OCTAL, ps);
            *ch = "";
            if (*next_ch == '{') {
                while (*next_ch != '}') {
                    if (*next_ch == '\0') {
                        res.code = BRU_PARSE_END_OF_STRING;
                        break;
                    }
                    next_ch++;
                }
            } else {
                // TODO: technically should be of length 1 <= n <= 3
                while (*next_ch < '8' && isdigit(*next_ch++));
                next_ch--;
            }
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'x':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_HEX, ps);
            *ch = "";
            if (*next_ch == '{') {
                while (*next_ch != '}') {
                    if (*next_ch == '\0') {
                        res.code = BRU_PARSE_END_OF_STRING;
                        break;
                    }
                    next_ch++;
                }
            } else {
                // TODO: technically should be of length 1 <= n <= 2
                while (isxdigit(*next_ch++));
                next_ch--;
            }
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        case 'u':
            FLAG_UNSUPPORTED(BRU_UNSUPPORTED_UNICODE, ps);
            *ch = "";
            if (*next_ch == '{') {
                while (*next_ch != '}') {
                    if (*next_ch == '\0') {
                        res.code = BRU_PARSE_END_OF_STRING;
                        break;
                    }
                    next_ch++;
                }
            } else {
                // TODO: technically should be of length 1 <= n <= 4
                while (isxdigit(*next_ch++));
                next_ch--;
            }
            res.code = BRU_PARSE_UNSUPPORTED;
            break;

        default:
            if (ispunct(*next_ch))
                *ch = next_ch;
            else
                res.code = BRU_PARSE_NO_MATCH;
    }

    if (SUCCEEDED(res.code)) ps->ch = ++next_ch;
    res.ch = ps->ch;
    return res;
}

#define PUSH_INTERVAL(start, end)                                \
    INTERVAL_LIST_ITEM_INIT(item, bru_interval((start), (end))); \
    BRU_DLL_PUSH_BACK(list->sentinel, item);                     \
    list->len++

#define PUSH_CHAR(ch) PUSH_INTERVAL(ch, ch);

static BruParseResult
parse_escape_cc(BruParseState *ps, BruIntervalList *list /**< out parameter */)
{
    BruIntervalListItem *item;
    BruParseResult       res = { BRU_PARSE_SUCCESS, NULL };
    const char          *next_ch;

    if (*(next_ch = ps->ch) != '\\')
        return PARSE_RES(BRU_PARSE_NO_MATCH, ps->ch);

    switch (*++next_ch) {
        case 'D':
            PUSH_INTERVAL("\x00", "\x2f");
            PUSH_INTERVAL("\x3a", UNICODE_END);
            break;

        case 'd': PUSH_INTERVAL("0", "9"); break;

        case 'N':
            PUSH_INTERVAL("\x00", "\x09");
            PUSH_INTERVAL("\x0b", UNICODE_END);
            break;

        case 'H':
            PUSH_INTERVAL("\x00", "\x08");
            PUSH_INTERVAL("\x10", "\x1f");
            PUSH_INTERVAL("\x21", UNICODE_END);
            break;

        case 'h':
            PUSH_CHAR("\t");
            PUSH_CHAR(" ");
            break;

        case 'V':
            PUSH_INTERVAL("\x00", "\x09");
            PUSH_INTERVAL("\x0e", UNICODE_END);
            break;

        case 'v': PUSH_INTERVAL("\x0a", "\x0d"); break;

        case 'S':
            PUSH_INTERVAL("\x00", "\x08");
            PUSH_INTERVAL("\x0e", "\x1f");
            PUSH_INTERVAL("\x21", UNICODE_END);
            break;

        case 's':
            PUSH_CHAR("\t");
            PUSH_INTERVAL("\x0a", "\x0d");
            PUSH_CHAR(" ");
            break;

        case 'W':
            PUSH_INTERVAL("\x00", "\x2f");
            PUSH_INTERVAL("\x3a", "\x40");
            PUSH_INTERVAL("\x5b", "\x5e");
            PUSH_CHAR("\x60");
            PUSH_INTERVAL("\x7b", UNICODE_END);
            break;

        case 'w':
            PUSH_INTERVAL("0", "9");
            PUSH_INTERVAL("A", "Z");
            PUSH_CHAR("_");
            PUSH_INTERVAL("a", "z");
            break;

        case 'C': /* fallthrough */
        case 'X': res.code = BRU_PARSE_INVALID_ESCAPE; break;

        default: res.code = BRU_PARSE_NO_MATCH; break;
    }

    if (SUCCEEDED(res.code)) ps->ch = ++next_ch;
    res.ch = ps->ch;
    return res;
}

static BruParseResult
parse_posix_cc(BruParseState *ps, BruIntervalList *list /**< out parameter */)
{
    BruIntervalListItem *item;
    BruParseResult       res = { BRU_PARSE_SUCCESS, NULL };

    if (strcmp(ps->ch, "[:alnum:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        PUSH_INTERVAL("a", "z");
        PUSH_INTERVAL("0", "9");
        ps->ch += sizeof("[:alnum:]") - 1;
    } else if (strcmp(ps->ch, "[:alpha:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        PUSH_INTERVAL("a", "z");
        ps->ch += sizeof("[:alpha:]") - 1;
    } else if (strcmp(ps->ch, "[:ascii:]") == 0) {
        PUSH_INTERVAL("\x00", "\x7f");
        ps->ch += sizeof("[:ascii:]") - 1;
    } else if (strcmp(ps->ch, "[:blank:]") == 0) {
        PUSH_CHAR(" ");
        PUSH_CHAR("\t");
        ps->ch += sizeof("[:blank:]") - 1;
    } else if (strcmp(ps->ch, "[:cntrl:]") == 0) {
        PUSH_INTERVAL("\x00", "\x1f");
        PUSH_CHAR("\x7f");
        ps->ch += sizeof("[:cntrl:]") - 1;
    } else if (strcmp(ps->ch, "[:digit:]") == 0) {
        PUSH_INTERVAL("0", "9");
        ps->ch += sizeof("[:digit:]") - 1;
    } else if (strcmp(ps->ch, "[:graph:]") == 0) {
        PUSH_INTERVAL("\x21", "\x7e");
        ps->ch += sizeof("[:graph:]") - 1;
    } else if (strcmp(ps->ch, "[:lower:]") == 0) {
        PUSH_INTERVAL("a", "z");
        ps->ch += sizeof("[:lower:]") - 1;
    } else if (strcmp(ps->ch, "[:print:]") == 0) {
        PUSH_INTERVAL("\x20", "\x7e");
        ps->ch += sizeof("[:print:]") - 1;
    } else if (strcmp(ps->ch, "[:punct:]") == 0) {
        PUSH_INTERVAL("!", "/");
        PUSH_INTERVAL(":", "@");
        PUSH_INTERVAL("[", "`");
        PUSH_INTERVAL("{", "~");
        ps->ch += sizeof("[:punct:]") - 1;
    } else if (strcmp(ps->ch, "[:space:]") == 0) {
        PUSH_CHAR(" ");
        PUSH_CHAR("\t");
        PUSH_CHAR("\f");
        PUSH_CHAR("\n");
        PUSH_CHAR("\r");
        PUSH_CHAR("\v");
        ps->ch += sizeof("[:space:]") - 1;
    } else if (strcmp(ps->ch, "[:upper:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        ps->ch += sizeof("[:upper:]") - 1;
    } else if (strcmp(ps->ch, "[:word:]") == 0) {
        PUSH_INTERVAL("A", "Z");
        PUSH_INTERVAL("a", "z");
        PUSH_INTERVAL("0", "9");
        PUSH_CHAR("_");
        ps->ch += sizeof("[:word:]") - 1;
    } else if (strcmp(ps->ch, "[:xdigit:]") == 0) {
        PUSH_INTERVAL("0", "9");
        PUSH_INTERVAL("A", "F");
        PUSH_INTERVAL("a", "f");
        ps->ch += sizeof("[:xdigit:]") - 1;
    } else {
        res.code = BRU_PARSE_NO_MATCH;
    }

    res.ch = ps->ch;
    return res;
}

#undef PUSH_CHAR
#undef PUSH_INTERVAL

static BruParseResult skip_comment(BruParseState *ps)
{
    const char *ch = ps->ch;

check_for_comment:
    if (*ch++ == '(' && *ch++ == '?' && *ch++ == '#') {
        while (*ch++ != ')')
            if (!*ch)
                return PARSE_RES(BRU_PARSE_INCOMPLETE_GROUP_STRUCTURE, ps->ch);
        ps->ch = ch;
        goto check_for_comment;
    }

    return PARSE_RES(BRU_PARSE_SUCCESS, ps->ch);
}

static void find_matching_closing_parenthesis(BruParseState *ps)
{
    size_t     nparen   = 1;
    bru_byte_t in_cc    = FALSE;
    bru_byte_t in_quote = FALSE;
    char       ch;

    while ((ch = *ps->ch)) {
        switch (ch) {
            case '[':
                if (!in_quote) in_cc = TRUE;
                break;

            case ']':
                if (in_cc) in_cc = FALSE;
                break;

            case ')':
                if (!in_cc && !in_quote) {
                    if (!(--nparen)) return;
                }
                break;

            case '(':
                if (!in_cc && !in_quote) nparen++;
                break;

            case '\\':
                ps->ch++;
                if (in_cc) break;
                if (*ps->ch == 'Q')
                    in_quote = TRUE;
                else if (*ps->ch == 'E' && in_quote)
                    in_quote = FALSE;
                break;

            default: break;
        }
        if (*ps->ch) ps->ch++;
    }
}

static void print_unsupported_feature(unsigned int feature_idx, FILE *stream)
{
    static const char *feature_strings[BRU_NUM_UNSUPPORTED_CODES] = {
        "UNSUPPORTED_BACKREF",
        "UNSUPPORTED_LOOKAHEAD",
        "UNSUPPORTED_OCTAL",
        "UNSUPPORTED_HEX",
        "UNSUPPORTED_UNICODE",
        "UNSUPPORTED_POSSESSIVE",
        "UNSUPPORTED_CONTROL_VERB",
        "UNSUPPORTED_FLAGS",
        "UNSUPPORTED_CONTROL_CODE",
        "UNSUPPORTED_LOOKBEHIND",
        "UNSUPPORTED_NAMED_GROUP",
        "UNSUPPORTED_RELATIVE_GROUP",
        "UNSUPPORTED_ATOMIC_GROUP",
        "UNSUPPORTED_PATTERN_RECURSION",
        "UNSUPPORTED_LOOKAHEAD_CONDITIONAL",
        "UNSUPPORTED_CALLOUT",
        "UNSUPPORTED_GROUP_RESET",
        "UNSUPPORTED_GROUP_RECURSION",
        "UNSUPPORTED_WORD_BOUNDARY",
        "UNSUPPORTED_START_BOUNDARY",
        "UNSUPPORTED_END_BOUNDARY",
        "UNSUPPORTED_FIRST_MATCH_BOUNDARY",
        "UNSUPPORTED_RESET_MATCH_START",
        "UNSUPPORTED_QUOTING",
        "UNSUPPORTED_UNICODE_PROPERTY",
        "UNSUPPORTED_NEWLINE_SEQUENCE"
    };

    if (feature_idx >= BRU_NUM_UNSUPPORTED_CODES) return;
    fprintf(stream, "%d: %s\n", feature_idx, feature_strings[feature_idx]);
}

static BruRegexNode *parser_regex_counter(BruRegexNode  *child,
                                          bru_byte_t     greedy,
                                          bru_cntr_t     min,
                                          bru_cntr_t     max,
                                          int            expand_counters,
                                          BruParseState *ps)
{
    BruRegexNode *counter, *left, *right, *tmp;
    bru_cntr_t    i;

    if (!expand_counters) {
        counter = bru_regex_counter(child, greedy, min, max);
        SET_RID(counter, ps);
    } else {
        left = min > 0 ? child : NULL;
        for (i = 1; i < min; i++) {
            left = bru_regex_branch(BRU_CONCAT, left, bru_regex_clone(child));
            SET_RID(left, ps);
        }

        right = max > min ? bru_regex_repetition(
                                BRU_QUES, left ? bru_regex_clone(child) : child,
                                greedy)
                          : NULL;
        SET_RID(right, ps);
        for (i = min + 1; i < max; i++) {
            tmp = bru_regex_branch(BRU_CONCAT, bru_regex_clone(child), right);
            SET_RID(tmp, ps);
            right = bru_regex_repetition(BRU_QUES, tmp, greedy);
            SET_RID(right, ps);
        }

        if (left && right) {
            counter = bru_regex_branch(BRU_CONCAT, left, right);
            SET_RID(counter, ps);
        } else {
            counter = left ? left : right;
        }
    }

    return counter;
}
