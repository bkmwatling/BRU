#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sre.h"
#include "utf8.h"
#include "utils.h"

#define BUF              2048
#define INTERVAL_MAX_BUF 13

#define STR_PUSH(s, len, alloc, str)                               \
    ENSURE_SPACE(s, (len) + strlen(str) + 1, alloc, sizeof(char)); \
    len += snprintf((s) + (len), (alloc) - (len), (str))

char *regex_to_tree_str_indent(Regex *re, int indent);

/* --- Interval ------------------------------------------------------------- */

Interval interval(int neg, const char *lbound, const char *ubound)
{
    Interval interval = { neg, lbound, ubound };
    return interval;
}

char *interval_to_str(Interval *interval)
{
    char *s = malloc((INTERVAL_MAX_BUF + 1) * sizeof(char)), *p = s, *q, *r;

    if (interval->neg) { *p++ = '^'; }

    q = utf8_to_str(interval->lbound);
    r = utf8_to_str(interval->ubound);

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

/* --- RegexTree ------------------------------------------------------------ */

Regex *regex_anchor(RegexType type)
{
    Regex *re = malloc(sizeof(Regex));

    /* check `type` to make sure correct node type */
    assert(type == CARET || type == DOLLAR);

    re->type = type;

    return re;
}

Regex *regex_literal(const char *ch)
{
    Regex *re = malloc(sizeof(Regex));

    re->type = LITERAL;
    re->ch   = ch;

    return re;
}

Regex *regex_cc(Interval *intervals, size_t len)
{
    Regex *re = malloc(sizeof(Regex));

    re->type      = CC;
    re->intervals = intervals;
    re->cc_len    = len;

    return re;
}

Regex *regex_branch(RegexType type, Regex *left, Regex *right)
{
    Regex *re = malloc(sizeof(Regex));

    /* check `type` to make sure correct node type */
    assert(type == ALT || type == CONCAT);

    re->type  = type;
    re->left  = left;
    re->right = right;

    return re;
}

Regex *regex_single_child(RegexType type, Regex *child, int pos)
{
    Regex *re = malloc(sizeof(Regex));

    /* check `type` to make sure correct node type */
    assert(type == CAPTURE || type == STAR || type == PLUS || type == QUES ||
           type == LOOKAHEAD);

    re->type = type;
    re->left = child;
    re->pos  = pos;

    return re;
}

Regex *regex_counter(Regex *child, int greedy, uint min, uint max)
{
    Regex *re = malloc(sizeof(Regex));

    re->type = COUNTER;
    re->left = child;
    re->pos  = greedy;
    re->min  = min;
    re->max  = max;

    return re;
}

char *regex_to_tree_str(Regex *re) { return regex_to_tree_str_indent(re, 0); }

char *regex_to_tree_str_indent(Regex *re, int indent)
{
    if (re == NULL) { return NULL; }

    size_t len = 0, alloc = BUF;
    char  *s = malloc(alloc * sizeof(char)), *p;

    ENSURE_SPACE(s, len + indent + 1, alloc, sizeof(char));
    len += snprintf(s + len, alloc - len, "%*s", indent, "");

    switch (re->type) {
        case CARET: STR_PUSH(s, len, alloc, "Caret"); break;

        case DOLLAR: STR_PUSH(s, len, alloc, "Dollar"); break;

        case LITERAL:
            ENSURE_SPACE(s, len + 14, alloc, sizeof(char));
            p    = utf8_to_str(re->ch);
            len += snprintf(s + len, alloc - len, "Literal(%s)", p);
            free(p);
            break;

        case CC:
            p = intervals_to_str(re->intervals, re->cc_len);
            ENSURE_SPACE(s, len + 12 + strlen(p), alloc, sizeof(char));
            len += snprintf(s + len, alloc - len, "CharClass(%s)", p);
            free(p);
            break;

        case ALT: STR_PUSH(s, len, alloc, "Alternation");
        case CONCAT:
            if (re->type == CONCAT) {
                STR_PUSH(s, len, alloc, "Concatenation");
            }
            if ((p = regex_to_tree_str_indent(re->left, indent + 2))) {
                ENSURE_SPACE(s, len + indent + strlen(p) + 7, alloc,
                             sizeof(char));
                len += snprintf(s + len, alloc - len, "\n%*sleft:\n%s", indent,
                                "", p);
                free(p);
            }

            if ((p = regex_to_tree_str_indent(re->right, indent + 2))) {
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
            if (re->type == STAR) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Star(%d)", re->pos);
            }
        case PLUS:
            if (re->type == PLUS) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Plus(%d)", re->pos);
            }
        case QUES:
            if (re->type == QUES) {
                ENSURE_SPACE(s, len + 9, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Ques(%d)", re->pos);
            }
        case COUNTER:
            if (re->type == COUNTER) {
                ENSURE_SPACE(s, len + 55, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Counter(%d, %u, ",
                                re->pos, re->min);
                if (re->max == UINT_MAX) {
                    len += snprintf(s + len, alloc - len, "inf)");
                } else {
                    len += snprintf(s + len, alloc - len, "%u)", re->max);
                }
            }
        case LOOKAHEAD:
            if (re->type == LOOKAHEAD) {
                ENSURE_SPACE(s, len + 14, alloc, sizeof(char));
                len += snprintf(s + len, alloc - len, "Lookahead(%d)", re->pos);
            }
            if ((p = regex_to_tree_str_indent(re->left, indent + 2))) {
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

void regex_free(Regex *re)
{
    switch (re->type) {
        case CARET:
        case DOLLAR:
        case LITERAL: break;

        case CC: free(re->intervals); break;

        case ALT:
        case CONCAT:
            regex_free(re->left);
            regex_free(re->right);
            break;

        case CAPTURE:
        case STAR:
        case PLUS:
        case QUES:
        case COUNTER:
        case LOOKAHEAD: regex_free(re->left); break;
    }

    free(re);
}
