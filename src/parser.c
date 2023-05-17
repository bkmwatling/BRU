#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

#define EOS "Reached end-of-string before completing regex!"

#define CONCAT(tree, node) tree ? regex_tree_branch(CONCAT, tree, node) : node

#define ALLOC_DOT                                 \
    Interval *dot = malloc(2 * sizeof(Interval)); \
    dot[0]        = interval(1, 0, 0);            \
    dot[1]        = interval(1, '\n', '\n')

RegexTree *parse_alt(Parser *p, uint idx, ParseState *ps);
RegexTree *parse_concat(Parser *p, uint idx, ParseState *ps);
RegexTree *parse_terminal(Parser *p, uint idx);
RegexTree *parse_cc(Parser *p, uint idx);
RegexTree *parse_predefined_cc(Parser *p, uint idx, int neg);
RegexTree *parse_parens(Parser *p, uint idx, ParseState *ps);
RegexTree *parse_curly(Parser *p, uint idx);

uint *str_to_regex(char *s)
{
    uint *regex = malloc((strlen(s) + 1) * sizeof(uint));
    uint *r     = regex;

    while (*s) { *r++ = (uint) *s++; }
    *r = 0;

    return regex;
}

ParseState *parse_state(int in_group, int in_lookahead)
{
    ParseState *ps   = malloc(sizeof(ParseState));
    ps->in_group     = in_group;
    ps->in_lookahead = in_lookahead;
    return ps;
}

Parser *parser(uint *regex,
               int   only_counters,
               int   unbounded_counters,
               int   whole_match_capture)
{
    Parser *p              = malloc(sizeof(Parser));
    p->regex               = regex;
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

RegexTree *parse_alt(Parser *p, uint idx, ParseState *ps) { return NULL; }
