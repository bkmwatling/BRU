#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STC_UTF_ENABLE_SHORT_NAMES
#include "stc/util/utf.h"

#include "parser.h"
#include "sre.h"
#include "utils.h"

#define EOS "Reached end-of-string before completing regex!"

#define PARSER_OPTS_DEFAULT ((ParserOpts){ 0, 0, 1 })

#define CONCAT(tree, node) \
    ((tree) ? regex_branch(CONCAT, (tree), (node)) : (node))

typedef struct {
    int in_group;
    int in_lookahead;
} ParseState;

static Regex *
parse_alt(const Parser *p, const char **const regex, ParseState *ps);
static Regex *
parse_concat(const Parser *p, const char **const regex, ParseState *ps);
static Regex    *parse_terminal(const char *ch, const char **const regex);
static Regex    *parse_cc(const char **const regex);
static Interval *parse_predefined_cc(const char **const regex,
                                     int                neg,
                                     Interval          *interval,
                                     len_t             *cc_len,
                                     len_t             *cc_alloc);
static Regex *
parse_parens(const Parser *p, const char **const regex, ParseState *ps);
static void parse_curly(const char **const regex, uint *min, uint *max);

static Interval *dot(void)
{
    Interval *dot = malloc(2 * sizeof(Interval));
    dot[0]        = interval(1, "\0", "\0"); /* XXX: untested */
    dot[1]        = interval(1, "\n", "\n");
    return dot;
}

Parser *parser_new(const char *regex, ParserOpts *opts)
{
    Parser *p = malloc(sizeof(Parser));
    p->regex  = malloc((strlen(regex) + 1) * sizeof(char));
    strcpy((char *) p->regex, regex);
    p->opts = opts ? *opts : PARSER_OPTS_DEFAULT;
    return p;
}

void parser_free(Parser *p)
{
    free((void *) p->regex);
    free(p);
}

Regex *parse(const Parser *p)
{
    const char *r  = p->regex;
    ParseState  ps = { 0, 0 };
    Regex      *re = parse_alt(p, &r, &ps);

    if (p->opts.whole_match_capture && re)
        re = regex_single_child(CAPTURE, re, 0);
    return re;
}

static Regex *
parse_alt(const Parser *p, const char **const regex, ParseState *ps)
{
    Regex *tree = NULL;

    while (**regex) {
        if (**regex == '|') {
            ++*regex;
            tree = regex_branch(ALT, tree, parse_concat(p, regex, ps));
        } else if (**regex == ')' && ps->in_group) {
            return tree;
        } else {
            tree = parse_concat(p, regex, ps);
        }
    }

    return tree;
}

static Regex *
parse_concat(const Parser *p, const char **const regex, ParseState *ps)
{
    Regex      *tree = NULL, *subtree = NULL;
    const char *ch;
    uint        min, max;
    int         greedy;

    while (**regex) {
        switch (*(ch = (*regex)++)) {
            case '|': --*regex; return tree;

            case '(': subtree = parse_parens(p, regex, ps); break;

            case '^': subtree = regex_anchor(CARET); break;

            case '$': subtree = regex_anchor(DOLLAR); break;

            default:
                --*regex;
                if (*ch == ')' && ps->in_group) {
                    return tree;
                } else {
                    *regex  += utf8_nbytes(ch);
                    subtree  = parse_terminal(ch, regex);
                }
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
                if (p->opts.only_counters) {
                    subtree = regex_counter(subtree, greedy, min, max);
                } else {
                    if (min == 0 && max == CNTR_MAX) {
                        subtree = regex_single_child(STAR, subtree, greedy);
                    } else if (min == 1 && max == CNTR_MAX) {
                        subtree = regex_single_child(PLUS, subtree, greedy);
                    } else if (min == 0 && max == 1) {
                        subtree = regex_single_child(QUES, subtree, greedy);
                    } else if (p->opts.unbounded_counters || max < CNTR_MAX) {
                        subtree = regex_counter(subtree, greedy, min, max);
                    } else {
                        subtree = regex_branch(
                            CONCAT, regex_counter(subtree, greedy, min, max),
                            regex_single_child(STAR, subtree, greedy));
                    }
                }
            }

            if (min != 0 || max != 0) tree = CONCAT(tree, subtree);
        }
    }

    return tree;
}

static Regex *parse_terminal(const char *ch, const char **const regex)
{
    len_t cc_len;

    switch (*ch) {
        case '[': return parse_cc(regex);

        case '\\':
            return regex_cc(
                parse_predefined_cc(regex, FALSE, NULL, &cc_len, NULL), cc_len);

        case '.': return regex_cc(dot(), 2);

        default: return regex_literal(ch);
    }
}

static Regex *parse_cc(const char **const regex)
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

    while (*(ch = *regex)) {
        *regex = utf8_str_next(*regex);
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
                            *regex += utf8_nbytes(*regex);
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

    if (**regex) {
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

        ch     = *regex;
        *regex = utf8_str_next(*regex);
        switch (*ch) {
            case 'd':
            case 'D':
                PUSH(intervals, *cc_len, *cc_alloc,
                     interval((*ch == 'D') != neg, "0", "9"));
                break;

            case 'N':
                PUSH(intervals, *cc_len, *cc_alloc, interval(!neg, "\n", "\n"));
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
    } else {
        return NULL;
    }
}

static Regex *
parse_parens(const Parser *p, const char **const regex, ParseState *ps)
{
    Regex      *tree;
    ParseState  ps_tmp;
    const char *ch;

    if (**regex == '?' && (*regex)[1]) {
        ++*regex;
        switch (*(ch = (*regex)++)) {
            case ':':
            case '|':
                ps_tmp = (ParseState){ TRUE, ps->in_lookahead };
                tree   = parse_alt(p, regex, &ps_tmp);
                break;

            case '=':
            case '!':
                ps_tmp = (ParseState){ TRUE, TRUE };
                if ((tree = parse_alt(p, regex, &ps_tmp)))
                    tree = regex_single_child(LOOKAHEAD, tree, *ch == '=');
                break;

            default:
                fprintf(stderr, "Invalid symbol after ? in parentheses!");
                exit(EXIT_FAILURE);
        }
    } else {
        ps_tmp = (ParseState){ TRUE, ps->in_lookahead };
        if ((tree = parse_alt(p, regex, &ps_tmp)) && !ps->in_lookahead)
            tree = regex_single_child(CAPTURE, tree, FALSE);
    }

    if (**regex == ')') ++*regex;

    return tree;
}

static void parse_curly(const char **const regex, uint *min, uint *max)
{
    const char *ch;
    int         process_min = TRUE;

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
            *max = *max * 10 + (*ch - '0');
        } else {
            switch (*ch) {
                case '}': return;

                default:
                    fprintf(stderr, "Invalid symbol in curly braces!");
                    exit(EXIT_FAILURE);
            }
        }
    }
}
