#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sre.h"
#include "utils.h"

#define BUF 2048

#define STR_PUSH(s, len, alloc, str)                             \
    ENSURE_SPACE(s, len + strlen(str) + 1, alloc, sizeof(char)); \
    len += sprintf(s + len, str)

#define INDENT_PUSH(s, len, alloc, indent)                      \
    ENSURE_SPACE(s, (len) + (indent) + 1, alloc, sizeof(char)); \
    for (i = 0; i < (indent); ++i) { (s)[(len)++] = ' '; }

Interval interval(int neg, utf8 lbound, utf8 ubound)
{
    Interval interval = { neg, lbound, ubound };
    return interval;
}

RegexTree *regex_tree_anchor(RegexKind kind)
{
    RegexTree *re_tree = malloc(sizeof(RegexTree));

    /* check `kind` to make sure correct node type */
    assert(kind == CARET || kind == DOLLAR);

    re_tree->kind = kind;

    return re_tree;
}

RegexTree *regex_tree_literal(utf8 ch)
{
    RegexTree *re_tree = malloc(sizeof(RegexTree));

    re_tree->kind = LITERAL;
    re_tree->ch   = ch;

    return re_tree;
}

RegexTree *regex_tree_cc(Interval *intervals, size_t len)
{
    RegexTree *re_tree = malloc(sizeof(RegexTree));

    re_tree->kind      = CHAR_CLASS;
    re_tree->intervals = intervals;
    re_tree->cc_len    = len;

    return re_tree;
}

RegexTree *regex_tree_branch(RegexKind kind, RegexTree *left, RegexTree *right)
{
    RegexTree *re_tree = malloc(sizeof(RegexTree));

    /* check `kind` to make sure correct node type */
    assert(kind == ALT || kind == CONCAT);

    re_tree->kind   = kind;
    re_tree->child  = left;
    re_tree->child_ = right;

    return re_tree;
}

RegexTree *regex_tree_single_child(RegexKind kind, RegexTree *child, int pos)
{
    RegexTree *re_tree = malloc(sizeof(RegexTree));

    /* check `kind` to make sure correct node type */
    assert(kind == CAPTURE || kind == STAR || kind == PLUS || kind == QUES ||
           kind == LOOKAHEAD);

    re_tree->kind  = kind;
    re_tree->child = child;
    re_tree->pos   = pos;

    return re_tree;
}

RegexTree *regex_tree_counter(RegexTree *child, int greedy, uint min, uint max)
{
    RegexTree *re_tree = malloc(sizeof(RegexTree));

    re_tree->kind  = COUNTER;
    re_tree->child = child;
    re_tree->pos   = greedy;
    re_tree->min   = min;
    re_tree->max   = max;

    return re_tree;
}

char *regex_tree_to_tree_str(RegexTree *re_tree, size_t indent)
{
    if (re_tree == NULL) { return NULL; }

    size_t i, len = 0, alloc = BUF;
    char  *s = malloc(alloc * sizeof(char)), *p;

    INDENT_PUSH(s, len, alloc, indent);

    switch (re_tree->kind) {
        case CARET: STR_PUSH(s, len, alloc, "Caret"); break;

        case DOLLAR: STR_PUSH(s, len, alloc, "Dollar"); break;

        case LITERAL:
            ENSURE_SPACE(s, len + 14, alloc, sizeof(char));
            p    = utf8_to_char(re_tree->ch);
            len += sprintf(s + len, "Literal(%s)", p);
            free(p);
            break;

        case CHAR_CLASS: /* TODO: */ break;

        case ALT: STR_PUSH(s, len, alloc, "Alternation\n");
        case CONCAT:
            if (re_tree->kind == CONCAT) {
                STR_PUSH(s, len, alloc, "Concatenation\n");
            }
            INDENT_PUSH(s, len, alloc, indent);
            if ((p = regex_tree_to_tree_str(re_tree->child, indent + 2))) {
                ENSURE_SPACE(s, len + strlen(p) + 7, alloc, sizeof(char));
                len += sprintf(s + len, "left:\n%s", p);
                free(p);
            }

            PUSH(s, len, alloc, '\n');
            INDENT_PUSH(s, len, alloc, indent);
            if ((p = regex_tree_to_tree_str(re_tree->child_, indent + 2))) {
                ENSURE_SPACE(s, len + strlen(p) + 8, alloc, sizeof(char));
                len += sprintf(s + len, "right:\n%s", p);
                free(p);
            }
            break;

        /* TODO: make prettier */
        case CAPTURE: STR_PUSH(s, len, alloc, "Capture\n");
        case STAR:
            if (re_tree->kind == STAR) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += sprintf(s + len, "Star(%d)\n", re_tree->pos);
            }
        case PLUS:
            if (re_tree->kind == PLUS) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += sprintf(s + len, "Plus(%d)\n", re_tree->pos);
            }
        case QUES:
            if (re_tree->kind == QUES) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += sprintf(s + len, "Ques(%d)\n", re_tree->pos);
            }
        case COUNTER:
            if (re_tree->kind == COUNTER) {
                ENSURE_SPACE(s, len + 55, alloc, sizeof(char));
                len += sprintf(s + len, "Counter(%d, %u, ", re_tree->pos,
                               re_tree->min);
                if (re_tree->max == UINT_MAX) {
                    len += sprintf(s + len, "inf)\n");
                } else {
                    len += sprintf(s + len, "%u)\n", re_tree->max);
                }
            }
        case LOOKAHEAD:
            if (re_tree->kind == LOOKAHEAD) {
                ENSURE_SPACE(s, len + 14, alloc, sizeof(char));
                len += sprintf(s + len, "Lookahead(%d)\n", re_tree->pos);
            }
            INDENT_PUSH(s, len, alloc, indent);
            if ((p = regex_tree_to_tree_str(re_tree->child, indent + 2))) {
                ENSURE_SPACE(s, len + strlen(p) + 7, alloc, sizeof(char));
                len += sprintf(s + len, "body:\n%s", p);
                free(p);
            }
            break;
    }

    return s;
}

void regex_tree_free(RegexTree *re_tree)
{
    switch (re_tree->kind) {
        case CARET:
        case DOLLAR:
        case LITERAL: break;

        case CHAR_CLASS: free(re_tree->intervals); break;

        case ALT:
        case CONCAT:
            regex_tree_free(re_tree->child);
            regex_tree_free(re_tree->child_);
            break;

        case CAPTURE:
        case STAR:
        case PLUS:
        case QUES:
        case COUNTER:
        case LOOKAHEAD: regex_tree_free(re_tree->child); break;
    }

    free(re_tree);
}
