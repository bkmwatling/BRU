#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "sre.h"

#define EOS "Reached end-of-string before completing regex!"

#define CONCAT(tree, node) tree ? regex_tree_branch(CONCAT, tree, node) : node

#define ENSURE_SPACE(intervals, len, cc_alloc)                           \
    if ((len) >= (cc_alloc)) {                                           \
        do {                                                             \
            cc_alloc *= 2;                                               \
        } while ((len) >= (cc_alloc));                                   \
        intervals = realloc((intervals), (cc_alloc) * sizeof(Interval)); \
    }

#define PUSH(intervals, cc_len, cc_alloc, interval)  \
    ENSURE_SPACE(intervals, (cc_len) + 1, cc_alloc); \
    intervals[cc_len++] = (interval)

RegexTree *parse_alt(Parser *p, size_t idx, ParseState *ps);
RegexTree *parse_concat(Parser *p, size_t idx, ParseState *ps);
RegexTree *parse_terminal(Parser *p, utf8 ch, size_t idx);
RegexTree *parse_cc(Parser *p, size_t idx);
Interval  *parse_predefined_cc(Parser *p, size_t idx, int neg, size_t *cc_len);
RegexTree *parse_parens(Parser *p, size_t idx, ParseState *ps);
RegexTree *parse_curly(Parser *p, size_t idx, uint *min, uint *max);

Interval *dot()
{
    Interval *dot = malloc(2 * sizeof(Interval));
    dot[0]        = interval(1, 0, 0);
    dot[1]        = interval(1, '\n', '\n');
    return dot;
}

ParseState *parse_state(int in_group, int in_lookahead)
{
    ParseState *ps   = malloc(sizeof(ParseState));
    ps->in_group     = in_group;
    ps->in_lookahead = in_lookahead;
    return ps;
}

Parser *parser(utf8 *regex,
               int   only_counters,
               int   unbounded_counters,
               int   whole_match_capture)
{
    Parser *p              = malloc(sizeof(Parser));
    p->regex               = regex;
    p->regex_len           = utf8len(regex);
    p->only_counters       = only_counters;
    p->unbounded_counters  = unbounded_counters;
    p->whole_match_capture = whole_match_capture;
    return p;
}

RegexTree *parse(Parser *p)
{
    ParseState ps      = { 0, 0 };
    RegexTree *re_tree = parse_alt(p, 0, &ps);

    if (p->whole_match_capture && re_tree) {
        re_tree = regex_tree_single_child(CAPTURE, re_tree, 0);
    }
    return re_tree;
}

RegexTree *parse_alt(Parser *p, size_t idx, ParseState *ps)
{
    RegexTree *tree = NULL;

    while (idx < p->regex_len) {
        switch (p->regex[idx]) {
            case '|':
                tree = regex_tree_branch(ALT, tree, parse_concat(p, ++idx, ps));
                break;

            default:
                if (p->regex[idx] == ')' && ps->in_group) {
                    return tree;
                } else {
                    tree = parse_concat(p, idx, ps);
                }
        }
    }

    return tree;
}

RegexTree *parse_concat(Parser *p, size_t idx, ParseState *ps)
{
    RegexTree *tree = NULL, *subtree = NULL;
    utf8       ch;
    uint       min, max;
    int        greedy;

    while (idx < p->regex_len) {
        switch ((ch = p->regex[idx])) {
            case '|': return tree;

            case '(': subtree = parse_parens(p, ++idx, ps); break;

            case '^':
                ++idx;
                subtree = regex_tree_anchor(CARET);
                break;

            case '$':
                ++idx;
                subtree = regex_tree_anchor(DOLLAR);
                break;

            default:
                if (ch == ')' && ps->in_group) {
                    return tree;
                } else {
                    subtree = parse_terminal(p, ch, ++idx);
                }
        }

        switch (p->regex[idx]) {
            case '*':
                ++idx;
                min = 0;
                max = UINT_MAX;
                break;

            case '+':
                ++idx;
                min = 1;
                max = UINT_MAX;
                break;

            case '?':
                ++idx;
                min = 0;
                max = 1;
                break;

            case '{': parse_curly(p, ++idx, &min, &max); break;

            default: min = 1; max = 1;
        }

        switch (p->regex[idx]) {
            case '?':
                ++idx;
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

RegexTree *parse_terminal(Parser *p, utf8 ch, size_t idx)
{
    size_t cc_len;

    switch (ch) {
        case '[': return parse_cc(p, idx);

        case '\\':
            return regex_tree_cc(parse_predefined_cc(p, idx, FALSE, &cc_len),
                                 cc_len);

        case '.': return regex_tree_cc(dot(), 2);

        default: return regex_tree_literal(ch);
    }
}

RegexTree *parse_cc(Parser *p, size_t idx)
{
    utf8      ch;
    int       neg;
    size_t    cc_alloc = 4, cc_len = 0, tmp_len;
    Interval *intervals = malloc(cc_alloc * sizeof(Interval)), *tmp;

    if ((neg = (idx < p->regex_len && p->regex[idx] == '^'))) { ++idx; }

    if (p->regex[idx] == ']') {
        ++idx;
        intervals[cc_len++] = interval(neg, ']', ']');
    }

    while (idx < p->regex_len) {
        switch ((ch = p->regex[idx++])) {
            case '-':
                PUSH(intervals, cc_len, cc_alloc, interval(neg, '-', '-'));
                break;

            case '\\':
                tmp = parse_predefined_cc(p, idx, neg, &tmp_len);
                ENSURE_SPACE(intervals, cc_len + tmp_len, cc_alloc);
                memcpy(intervals + cc_len, tmp, tmp_len * sizeof(Interval));
                free(tmp);
                break;

            case ']': return regex_tree_cc(intervals, cc_len);

            default:
                if (idx + 1 < p->regex_len && p->regex[idx] == '-') {
                    switch (p->regex[++idx]) {
                        case '\\':
                            if (idx + 1 < p->regex_len &&
                                p->regex[idx + 1] == '\\') {
                                ++idx;
                                PUSH(intervals, cc_len, cc_alloc,
                                     interval(neg, ch, '\\'));
                                break;
                            }
                        case ']':
                            PUSH(intervals, cc_len, cc_alloc,
                                 interval(neg, ch, ch));
                            PUSH(intervals, cc_len, cc_alloc,
                                 interval(neg, '-', '-'));
                            break;

                        default:
                            PUSH(intervals, cc_len, cc_alloc,
                                 interval(neg, ch, p->regex[idx++]));
                    }
                } else if (idx < p->regex_len) {
                    PUSH(intervals, cc_len, cc_alloc, interval(neg, ch, ch));
                }
        }
    }

    free(intervals);
    return NULL;
}
