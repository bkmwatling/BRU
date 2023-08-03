#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "sre.h"
#include "utf8.h"
#include "utils.h"

#define EOS "Reached end-of-string before completing regex!"

#define CONCAT(tree, node) tree ? regex_tree_branch(CONCAT, tree, node) : node

RegexTree *parse_alt(Parser *p, const char **const regex, ParseState *ps);
RegexTree *parse_concat(Parser *p, const char **const regex, ParseState *ps);
RegexTree *parse_terminal(const char *ch, const char **const regex);
RegexTree *parse_cc(const char **const regex);
Interval  *parse_predefined_cc(const char **const regex,
                               int                neg,
                               Interval          *interval,
                               size_t            *cc_len,
                               size_t            *cc_alloc);
RegexTree *parse_parens(Parser *p, const char **const regex, ParseState *ps);
void       parse_curly(const char **const regex, uint *min, uint *max);

Interval *dot(void)
{
    Interval *dot = malloc(2 * sizeof(Interval));
    dot[0]        = interval(1, "\0", "\0"); /* XXX: untested */
    dot[1]        = interval(1, "\n", "\n");
    return dot;
}

ParseState *parse_state(int in_group, int in_lookahead)
{
    ParseState *ps   = malloc(sizeof(ParseState));
    ps->in_group     = in_group;
    ps->in_lookahead = in_lookahead;
    return ps;
}

Parser *parser(const char *regex,
               int         only_counters,
               int         unbounded_counters,
               int         whole_match_capture)
{
    Parser *p              = malloc(sizeof(Parser));
    p->regex               = regex;
    p->only_counters       = only_counters;
    p->unbounded_counters  = unbounded_counters;
    p->whole_match_capture = whole_match_capture;
    return p;
}

void parser_free(Parser *p)
{
    free((void *) p->regex);
    free(p);
}

RegexTree *parse(Parser *p)
{
    const char *r       = p->regex;
    ParseState  ps      = { 0, 0 };
    RegexTree  *re_tree = parse_alt(p, &r, &ps);

    if (p->whole_match_capture && re_tree) {
        re_tree = regex_tree_single_child(CAPTURE, re_tree, 0);
    }
    return re_tree;
}

RegexTree *parse_alt(Parser *p, const char **const regex, ParseState *ps)
{
    RegexTree *tree = NULL;

    while (**regex) {
        if (**regex == '|') {
            ++*regex;
            tree = regex_tree_branch(ALT, tree, parse_concat(p, regex, ps));
        } else if (**regex == ')' && ps->in_group) {
            return tree;
        } else {
            tree = parse_concat(p, regex, ps);
        }
    }

    return tree;
}

RegexTree *parse_concat(Parser *p, const char **const regex, ParseState *ps)
{
    RegexTree  *tree = NULL, *subtree = NULL;
    const char *ch;
    uint        min, max;
    int         greedy;

    while (**regex) {
        switch (*(ch = (*regex)++)) {
            case '|': --*regex; return tree;

            case '(': subtree = parse_parens(p, regex, ps); break;

            case '^': subtree = regex_tree_anchor(CARET); break;

            case '$': subtree = regex_tree_anchor(DOLLAR); break;

            default:
                --*regex;
                if (*ch == ')' && ps->in_group) {
                    return tree;
                } else {
                    *regex  += utf8_num_bytes(ch);
                    subtree  = parse_terminal(ch, regex);
                }
        }

        switch (*(*regex)++) {
            case '*':
                min = 0;
                max = UINT_MAX;
                break;

            case '+':
                min = 1;
                max = UINT_MAX;
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
                if (p->only_counters) {
                    subtree = regex_tree_counter(subtree, greedy, min, max);
                } else {
                    if (min == 0 && max == UINT_MAX) {
                        subtree =
                            regex_tree_single_child(STAR, subtree, greedy);
                    } else if (min == 1 && max == UINT_MAX) {
                        subtree =
                            regex_tree_single_child(PLUS, subtree, greedy);
                    } else if (min == 0 && max == 1) {
                        subtree =
                            regex_tree_single_child(QUES, subtree, greedy);
                    } else if (p->unbounded_counters || max < UINT_MAX) {
                        subtree = regex_tree_counter(subtree, greedy, min, max);
                    } else {
                        subtree = regex_tree_branch(
                            CONCAT,
                            regex_tree_counter(subtree, greedy, min, max),
                            regex_tree_single_child(STAR, subtree, greedy));
                    }
                }
            }

            if (min != 0 || max != 0) { tree = CONCAT(tree, subtree); }
        }
    }

    return tree;
}

RegexTree *parse_terminal(const char *ch, const char **const regex)
{
    size_t cc_len;

    switch (*ch) {
        case '[': return parse_cc(regex);

        case '\\':
            return regex_tree_cc(
                parse_predefined_cc(regex, FALSE, NULL, &cc_len, NULL), cc_len);

        case '.': return regex_tree_cc(dot(), 2);

        default: return regex_tree_literal(ch);
    }
}

RegexTree *parse_cc(const char **const regex)
{
    const char *ch;
    int         neg;
    size_t      cc_alloc = 4, cc_len = 0;
    Interval   *intervals = malloc(cc_alloc * sizeof(Interval));

    if ((neg = **regex == '^')) { ++*regex; }

    if (**regex == ']') {
        ++*regex;
        intervals[cc_len++] = interval(neg, "]", "]");
    }

    while (**regex) {
        switch (*(ch = (*regex)++)) {
            case '-':
                PUSH(intervals, cc_len, cc_alloc, interval(neg, "-", "-"));
                break;

            case '\\':
                parse_predefined_cc(regex, neg, intervals, &cc_len, &cc_alloc);
                break;

            case ']': return regex_tree_cc(intervals, cc_len);

            default:
                --*regex;
                *regex += utf8_num_bytes(ch);
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
                            *regex += utf8_num_bytes(*regex);
                    }
                } else if (**regex) {
                    PUSH(intervals, cc_len, cc_alloc, interval(neg, ch, ch));
                }
        }
    }

    free(intervals);
    return NULL;
}

Interval *parse_predefined_cc(const char **const regex,
                              int                neg,
                              Interval          *intervals,
                              size_t            *cc_len,
                              size_t            *cc_alloc)
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
            if (*cc_alloc == 0) { *cc_alloc = 4; }
            intervals = malloc(*cc_alloc * sizeof(Interval));
        }

        switch (*(ch = (*regex)++)) {
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

            default:
                --*regex;
                *regex += utf8_num_bytes(ch);
                PUSH(intervals, *cc_len, *cc_alloc, interval(neg, ch, ch));
        }

        if (cc_len_null) { free(cc_len); }
        if (cc_alloc_null) { free(cc_alloc); }

        return intervals;
    } else {
        return NULL;
    }
}

RegexTree *parse_parens(Parser *p, const char **const regex, ParseState *ps)
{
    RegexTree  *tree;
    ParseState *ps_tmp;
    const char *ch;

    if (**regex == '?' && (*regex)[1]) {
        ++*regex;
        switch (*(ch = (*regex)++)) {
            case ':':
            case '|':
                ps_tmp = parse_state(TRUE, ps->in_lookahead);
                tree   = parse_alt(p, regex, ps_tmp);
                free(ps_tmp);
                break;

            case '=':
            case '!':
                ps_tmp = parse_state(TRUE, TRUE);
                if ((tree = parse_alt(p, regex, ps_tmp))) {
                    tree = regex_tree_single_child(LOOKAHEAD, tree, *ch == '=');
                }
                free(ps_tmp);
                break;

            default:
                fprintf(stderr, "Invalid symbol after ? in parentheses!");
                exit(EXIT_FAILURE);
        }
    } else {
        ps_tmp = parse_state(TRUE, ps->in_lookahead);
        if ((tree = parse_alt(p, regex, ps_tmp)) && !ps->in_lookahead) {
            tree = regex_tree_single_child(CAPTURE, tree, FALSE);
        }
        free(ps_tmp);
    }

    if (**regex == ')') { ++*regex; }

    return tree;
}

void parse_curly(const char **const regex, uint *min, uint *max)
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
