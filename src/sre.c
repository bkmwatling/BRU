#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sre.h"
#include "utils.h"

#define BUF              2048
#define INTERVAL_MAX_BUF 13

#define STR_PUSH(s, len, alloc, str)                             \
    ENSURE_SPACE(s, len + strlen(str) + 1, alloc, sizeof(char)); \
    len += snprintf((s) + (len), (alloc) - (len), (str))

char *regex_tree_to_tree_str_indent(RegexTree *re_tree, int indent);

Interval interval(int neg, utf8 lbound, utf8 ubound)
{
    Interval interval = { neg, lbound, ubound };
    return interval;
}

char *interval_to_str(Interval *interval)
{
    char *s = malloc((INTERVAL_MAX_BUF + 1) * sizeof(char)), *p = s, *q, *r;

    if (interval->neg) { *p++ = '^'; }

    q  = utf8_to_char(interval->lbound);
    r  = utf8_to_char(interval->ubound);
    p += snprintf(p, INTERVAL_MAX_BUF + 1, "(%s, %s)", q, r);
    free(q);
    free(r);

    return s;
}

char *intervals_to_str(Interval *intervals, size_t len)
{
    size_t i, slen = 0, alloc = 3 + 2 * INTERVAL_MAX_BUF;
    char  *s = malloc(alloc * sizeof(char)), *p;

    s[slen++] = '[';
    for (i = 0; i < len; ++i) {
        p = interval_to_str(intervals + i);
        ENSURE_SPACE(s, slen + INTERVAL_MAX_BUF + 3, alloc, sizeof(char));
        if (i < len - 1) {
            slen += snprintf(s + slen, alloc - slen, "%s, ", p);
        } else {
            slen += snprintf(s + slen, alloc - slen, "%s", p);
        }
        free(p);
    }
    ENSURE_SPACE(s, slen + 2, alloc, sizeof(char));
    s[slen++] = ']';
    s[slen]   = '\0';

    return s;
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

char *regex_tree_to_tree_str(RegexTree *re_tree)
{
    return regex_tree_to_tree_str_indent(re_tree, 0);
}

char *regex_tree_to_tree_str_indent(RegexTree *re_tree, int indent)
{
    if (re_tree == NULL) { return NULL; }

    size_t len = 0, alloc = BUF;
    char  *s = malloc(alloc * sizeof(char)), *p;

    ENSURE_SPACE(s, len + indent + 1, alloc, sizeof(char));
    len += snprintf(s + len, alloc - len, "%*s", indent, "");

    switch (re_tree->kind) {
        case CARET: STR_PUSH(s, len, alloc, "Caret"); break;

        case DOLLAR: STR_PUSH(s, len, alloc, "Dollar"); break;

        case LITERAL:
            ENSURE_SPACE(s, len + 14, alloc, sizeof(char));
            p    = utf8_to_char(re_tree->ch);
            len += snprintf(s + len, alloc - len, "Literal(%s)", p);
            free(p);
            break;

        case CHAR_CLASS:
            p = intervals_to_str(re_tree->intervals, re_tree->cc_len);
            ENSURE_SPACE(s, len + 12 + strlen(p), alloc, sizeof(char));
            len += snprintf(s + len, alloc - len, "CharClass(%s)", p);
            free(p);
            break;

        case ALT: STR_PUSH(s, len, alloc, "Alternation");
        case CONCAT:
            if (re_tree->kind == CONCAT) {
                STR_PUSH(s, len, alloc, "Concatenation");
            }
            if ((p = regex_tree_to_tree_str_indent(re_tree->child,
                                                   indent + 2))) {
                ENSURE_SPACE(s, len + indent + strlen(p) + 7, alloc,
                             sizeof(char));
                len += snprintf(s + len, alloc - len, "\n%*sleft:\n%s", indent,
                                "", p);
                free(p);
            }

            if ((p = regex_tree_to_tree_str_indent(re_tree->child_,
                                                   indent + 2))) {
                ENSURE_SPACE(s, len + indent + strlen(p) + 8, alloc,
                             sizeof(char));
                len += snprintf(s + len, alloc - len, "\n%*sright:\n%s", indent,
                                "", p);
                free(p);
            }
            break;

        /* TODO: make prettier */
        case CAPTURE: STR_PUSH(s, len, alloc, "Capture");
        case STAR:
            if (re_tree->kind == STAR) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Star(%d)", re_tree->pos);
            }
        case PLUS:
            if (re_tree->kind == PLUS) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Plus(%d)", re_tree->pos);
            }
        case QUES:
            if (re_tree->kind == QUES) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Ques(%d)", re_tree->pos);
            }
        case COUNTER:
            if (re_tree->kind == COUNTER) {
                ENSURE_SPACE(s, len + 55, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Counter(%d, %u, ",
                                re_tree->pos, re_tree->min);
                if (re_tree->max == UINT_MAX) {
                    len += snprintf(s + len, alloc - len, "inf)");
                } else {
                    len += snprintf(s + len, alloc - len, "%u)", re_tree->max);
                }
            }
        case LOOKAHEAD:
            if (re_tree->kind == LOOKAHEAD) {
                ENSURE_SPACE(s, len + 14, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Lookahead(%d)",
                                re_tree->pos);
            }
            if ((p = regex_tree_to_tree_str_indent(re_tree->child,
                                                   indent + 2))) {
                ENSURE_SPACE(s, len + indent + strlen(p) + 7, alloc,
                             sizeof(char));
                len += snprintf(s + len, alloc - len, "\n%*sbody:\n%s", indent,
                                "", p);
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
