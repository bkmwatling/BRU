#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "stc/util/utf.h"

#include "parser.h"
#include "sre.h"
#include "utils.h"

#define EOS "Reached end-of-string before completing regex!"

#define PARSER_OPTS_DEFAULT ((ParserOpts){ 0, 0, 0, 1 })

#define SET_RID(node, ps) \
    if (node) (node)->rid = (ps)->next_rid++

typedef struct {
    int      in_group;
    int      in_lookahead;
    len_t    ncaptures;
    regex_id next_rid;
} ParseState;

static RegexNode *
parse_alt(const Parser *self, const char **const regex, ParseState *ps);
static RegexNode *
parse_concat(const Parser *self, const char **const regex, ParseState *ps);
static RegexNode *parse_terminal(const Parser      *self,
                                 const char        *ch,
                                 const char **const regex,
                                 ParseState        *ps);
static RegexNode *parse_cc(const char **const regex);
static Interval  *parse_predefined_cc(const char **const regex,
                                      int                neg,
                                      Interval          *interval,
                                      len_t             *cc_len,
                                      len_t             *cc_alloc);
static RegexNode *
parse_parens(const Parser *self, const char **const regex, ParseState *ps);
static void parse_curly(const char **const regex, cntr_t *min, cntr_t *max);
static RegexNode *parser_regex_counter(RegexNode  *child,
                                       byte        greedy,
                                       cntr_t      min,
                                       cntr_t      max,
                                       int         expand_counters,
                                       ParseState *ps);

static Interval *dot(void)
{
    Interval *dot = malloc(2 * sizeof(Interval));
    dot[0]        = interval(1, "\0", "\0");
    dot[1]        = interval(1, "\n", "\n");
    return dot;
}

Parser *parser_new(const char *regex, const ParserOpts *opts)
{
    Parser *parser = malloc(sizeof(Parser));
    parser->regex  = regex;
    parser->opts   = opts ? *opts : PARSER_OPTS_DEFAULT;
    return parser;
}

void parser_free(Parser *self) { free(self); }

Regex parser_parse(const Parser *self)
{
    const char *r  = self->regex;
    ParseState  ps = { 0, 0, self->opts.whole_match_capture ? 1 : 0, 0 };
    RegexNode  *re = parse_alt(self, &r, &ps);

    if (self->opts.whole_match_capture && re) {
        re      = regex_capture(re, 0);
        re->rid = ps.next_rid++;
    }

    return (Regex){ self->regex, re };
}

static RegexNode *
parse_alt(const Parser *self, const char **const regex, ParseState *ps)
{
    RegexNode *tree = NULL;

    while (**regex) {
        if (**regex == ')' && ps->in_group) {
            break;
        } else if (**regex == '|') {
            ++*regex;
            tree = regex_branch(ALT, tree, parse_concat(self, regex, ps));
            SET_RID(tree, ps);
        } else {
            tree = parse_concat(self, regex, ps);
        }
    }

    return tree;
}

static RegexNode *
parse_concat(const Parser *self, const char **const regex, ParseState *ps)
{
    RegexNode  *tree = NULL, *subtree = NULL, *tmp;
    const char *ch;
    cntr_t      min, max;
    int         greedy;

    while (**regex) {
        switch (*(ch = stc_utf8_str_advance(regex))) {
            case '|': *regex = ch; return tree;

            case ')':
                if (ps->in_group) {
                    *regex = ch;
                    return tree;
                }

            default: subtree = parse_terminal(self, ch, regex, ps);
        }

        switch (*(*regex)++) {
            case '*':
                min = 0;
                max = CNTR_MAX;
                break;

            case '+':
                min = 1;
                max = CNTR_MAX;
                break;

            case '?':
                min = 0;
                max = 1;
                break;

            case '{': parse_curly(regex, &min, &max); break;

            default:
                --*regex;
                min = 1;
                max = 1;
        }

        switch (**regex) {
            case '?':
                ++*regex;
                greedy = FALSE;
                break;

            default: greedy = TRUE;
        }

        if (subtree) {
            if (min != 1 || max != 1) {
                switch (subtree->type) {
                    case CARET:
                    case DOLLAR:
                    case MEMOISE:
                        fprintf(stderr, "Token not quantifiable!\n");
                        exit(EXIT_FAILURE);

                    case LITERAL:
                    case CC:
                    case ALT:
                    case CONCAT:
                    case CAPTURE:
                    case STAR:
                    case PLUS:
                    case QUES:
                    case COUNTER:
                    case LOOKAHEAD: break;
                    case NREGEXTYPES: assert(0 && "unreachable");
                }

                if (self->opts.only_counters) {
                    subtree = regex_counter(subtree, greedy, min, max);
                    SET_RID(subtree, ps);
                } else if (min == 0 && max == CNTR_MAX) {
                    subtree = regex_single_child(STAR, subtree, greedy);
                    SET_RID(subtree, ps);
                } else if (min == 1 && max == CNTR_MAX) {
                    subtree = regex_single_child(PLUS, subtree, greedy);
                    SET_RID(subtree, ps);
                } else if (min == 0 && max == 1) {
                    subtree = regex_single_child(QUES, subtree, greedy);
                    SET_RID(subtree, ps);
                } else if (self->opts.unbounded_counters || max < CNTR_MAX) {
                    subtree =
                        parser_regex_counter(subtree, greedy, min, max,
                                             self->opts.expand_counters, ps);
                } else {
                    tmp = regex_single_child(STAR, subtree, greedy);
                    subtree =
                        regex_branch(CONCAT,
                                     parser_regex_counter(
                                         regex_clone(subtree), greedy, min, min,
                                         self->opts.expand_counters, ps),
                                     tmp);
                    SET_RID(tmp, ps);
                    SET_RID(subtree, ps);
                }
            }

            if (min != 0 || max != 0) {
                if (tree) {
                    tree = regex_branch(CONCAT, tree, subtree);
                    SET_RID(tree, ps);
                } else {
                    tree = subtree;
                }
            }
        }
    }

    return tree;
}

static RegexNode *parse_terminal(const Parser      *self,
                                 const char        *ch,
                                 const char **const regex,
                                 ParseState        *ps)
{
    len_t      cc_len;
    RegexNode *terminal;

    switch (*ch) {
        case '[': terminal = parse_cc(regex); break;

        case '\\':
            terminal = regex_cc(
                parse_predefined_cc(regex, FALSE, NULL, &cc_len, NULL), cc_len);
            break;

        case '(': return parse_parens(self, regex, ps);

        case '^':
            while (**regex == '^') ++*regex;
            terminal = regex_anchor(CARET);
            break;

        case '$':
            while (**regex == '$') ++*regex;
            terminal = regex_anchor(DOLLAR);
            break;

        // NOTE: not really an anchor, but works in much the same way
        case '#':
            while (**regex == '#') ++*regex;
            terminal = regex_anchor(MEMOISE);
            break;

        case '.': terminal = regex_cc(dot(), 2); break;

        default: terminal = regex_literal(ch); break;
    }

    SET_RID(terminal, ps);
    return terminal;
}

static RegexNode *parse_cc(const char **const regex)
{
    const char *ch;
    int         neg;
    len_t       cc_alloc = 4, cc_len = 0;
    Interval   *intervals = malloc(cc_alloc * sizeof(Interval));

    if ((neg = **regex == '^')) ++*regex;

    if (**regex == ']') {
        ++*regex;
        intervals[cc_len++] = interval(neg, "]", "]");
    }

    while (*(ch = stc_utf8_str_advance(regex))) {
        switch (*ch) {
            case '-':
                PUSH(intervals, cc_len, cc_alloc, interval(neg, "-", "-"));
                break;

            case '\\':
                parse_predefined_cc(regex, neg, intervals, &cc_len, &cc_alloc);
                break;

            case ']': return regex_cc(intervals, cc_len);

            default:
                if (**regex == '-' && (*regex)[1]) {
                    switch (*(++*regex)) {
                        case '\\':
                            if ((*regex)[1] == '\\') {
                                ++*regex;
                                PUSH(intervals, cc_len, cc_alloc,
                                     interval(neg, ch, "\\"));
                                break;
                            } else {
                                parse_predefined_cc(regex, neg, intervals,
                                                    &cc_len, &cc_alloc);
                            }
                        case ']':
                            PUSH(intervals, cc_len, cc_alloc,
                                 interval(neg, ch, ch));
                            PUSH(intervals, cc_len, cc_alloc,
                                 interval(neg, "-", "-"));
                            break;

                        default:
                            PUSH(intervals, cc_len, cc_alloc,
                                 interval(neg, ch, *regex));
                            *regex += stc_utf8_nbytes(*regex);
                    }
                } else if (**regex) {
                    PUSH(intervals, cc_len, cc_alloc, interval(neg, ch, ch));
                }
        }
    }

    free(intervals);
    return NULL;
}

static Interval *parse_predefined_cc(const char **const regex,
                                     int                neg,
                                     Interval          *intervals,
                                     len_t             *cc_len,
                                     len_t             *cc_alloc)
{
    const char *ch;
    int         cc_len_null = FALSE, cc_alloc_null = FALSE;

    if (**regex == '\0') return NULL;

    if (cc_len == NULL) {
        cc_len_null = TRUE;
        cc_len      = malloc(sizeof(size_t));
        *cc_len     = 0;
    }

    if (cc_alloc == NULL) {
        cc_alloc_null = TRUE;
        cc_alloc      = malloc(sizeof(size_t));
        *cc_alloc     = 4;
    }

    if (intervals == NULL) {
        *cc_len = 0;
        if (*cc_alloc == 0) *cc_alloc = 4;
        intervals = malloc(*cc_alloc * sizeof(Interval));
    }

    switch (*(ch = stc_utf8_str_advance(regex))) {
        case 'd':
        case 'D':
            PUSH(intervals, *cc_len, *cc_alloc,
                 interval((*ch == 'D') != neg, "0", "9"));
            break;

        case 'n':
        case 'N':
            PUSH(intervals, *cc_len, *cc_alloc,
                 interval((*ch == 'N') != neg, "\n", "\n"));
            break;

        case 'h':
        case 'H':
            neg = (*ch == 'H') != neg;
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, " ", " "));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\t", "\t"));
            break;

        case 'v':
        case 'V':
            neg = (*ch == 'V') != neg;
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\f", "\f"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\n", "\n"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\r", "\r"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\v", "\v"));
            break;

        case 's':
        case 'S':
            neg = (*ch == 'S') != neg;
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, " ", " "));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\t", "\t"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\f", "\f"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\n", "\n"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\r", "\r"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "\v", "\v"));
            break;

        case 'w':
        case 'W':
            neg = (*ch == 'W') != neg;
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "a", "z"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "A", "Z"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "_", "_"));
            PUSH(intervals, *cc_len, *cc_alloc, interval(neg, "0", "9"));
            break;

        default: PUSH(intervals, *cc_len, *cc_alloc, interval(neg, ch, ch));
    }

    if (cc_len_null) free(cc_len);
    if (cc_alloc_null) free(cc_alloc);

    return intervals;
}

static RegexNode *
parse_parens(const Parser *self, const char **const regex, ParseState *ps)
{
    RegexNode  *tree;
    ParseState  ps_tmp;
    const char *ch;
    len_t       ncaptures;

    if (**regex == '?' && (*regex)[1]) {
        ++*regex;
        switch (*(ch = (*regex)++)) {
            case ':':
            case '|':
                ps_tmp = (ParseState){ TRUE, ps->in_lookahead, ps->ncaptures,
                                       ps->next_rid };
                tree   = parse_alt(self, regex, &ps_tmp);
                ps->ncaptures = ps_tmp.ncaptures;
                ps->next_rid  = ps_tmp.next_rid;
                break;

            case '=':
            case '!':
                // captures aren't recorded in lookaheads
                ps_tmp = (ParseState){ TRUE, TRUE, 0, ps->next_rid };
                if ((tree = parse_alt(self, regex, &ps_tmp))) {
                    ps->next_rid = ps_tmp.next_rid;
                    tree = regex_single_child(LOOKAHEAD, tree, *ch == '=');
                    SET_RID(tree, ps);
                }
                break;

            default:
                fprintf(stderr, "Invalid symbol after ? in parentheses!");
                exit(EXIT_FAILURE);
        }
    } else {
        if (!ps->in_lookahead) ncaptures = ps->ncaptures++;
        ps_tmp =
            (ParseState){ TRUE, ps->in_lookahead, ps->ncaptures, ps->next_rid };
        if ((tree = parse_alt(self, regex, &ps_tmp)) && !ps->in_lookahead) {
            ps->next_rid = ps_tmp.next_rid;
            tree         = regex_capture(tree, ncaptures);
            SET_RID(tree, ps);
        }
        ps->ncaptures = ps_tmp.ncaptures;
    }

    if (**regex == ')') ++*regex;

    return tree;
}

static void parse_curly(const char **const regex, cntr_t *min, cntr_t *max)
{
    const char *ch;
    int         process_min = TRUE, max_empty = TRUE;

    *min = 0;
    while (**regex && process_min) {
        if (*(ch = (*regex)++) >= '0' && *ch <= '9') {
            *min = *min * 10 + (*ch - '0');
        } else {
            switch (*ch) {
                case ',': process_min = FALSE; break;

                case '}': *max = *min; return;

                default:
                    fprintf(stderr, "Invalid symbol in curly braces!");
                    exit(EXIT_FAILURE);
            }
        }
    }

    *max = 0;
    while (**regex) {
        if (*(ch = (*regex)++) >= '0' && *ch <= '9') {
            max_empty = FALSE;
            *max      = *max * 10 + (*ch - '0');
        } else {
            switch (*ch) {
                case '}':
                    if (max_empty) *max = CNTR_MAX;
                    return;

                default:
                    fprintf(stderr, "Invalid symbol in curly braces!");
                    exit(EXIT_FAILURE);
            }
        }
    }
}

static RegexNode *parser_regex_counter(RegexNode  *child,
                                       byte        greedy,
                                       cntr_t      min,
                                       cntr_t      max,
                                       int         expand_counters,
                                       ParseState *ps)
{
    RegexNode *counter, *left, *right, *tmp;
    cntr_t     i;

    if (!expand_counters) {
        counter = regex_counter(child, greedy, min, max);
        SET_RID(counter, ps);
    } else {
        left = min > 0 ? child : NULL;
        for (i = 1; i < min; i++) {
            left = regex_branch(CONCAT, left, regex_clone(child));
            SET_RID(left, ps);
        }

        right = max > min ? regex_single_child(
                                QUES, left ? regex_clone(child) : child, greedy)
                          : NULL;
        SET_RID(right, ps);
        for (i = min + 1; i < max; i++) {
            tmp = regex_branch(CONCAT, regex_clone(child), right);
            SET_RID(tmp, ps);
            right = regex_single_child(QUES, tmp, greedy);
            SET_RID(right, ps);
        }

        if (left && right) {
            counter = regex_branch(CONCAT, left, right);
            SET_RID(counter, ps);
        } else {
            counter = left ? left : right;
        }
    }

    return counter;
}
