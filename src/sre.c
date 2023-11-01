#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STC_UTF_ENABLE_SHORT_NAMES
#include "stc/util/utf.h"

#include "sre.h"
#include "utils.h"

#define BUF              512
#define INTERVAL_MAX_BUF 13

static void regex_to_tree_str_indent(char   *s,
                                     size_t *len,
                                     size_t *alloc,
                                     Regex  *re,
                                     int     indent);

/* --- Interval ------------------------------------------------------------- */

Interval interval(int neg, const char *lbound, const char *ubound)
{
    Interval interval = { neg, lbound, ubound };
    return interval;
}

char *interval_to_str(Interval *interval)
{
    char *s = malloc((INTERVAL_MAX_BUF + 1) * sizeof(char)), *p = s;

    if (interval->neg) *p++ = '^';
    p += snprintf(p, INTERVAL_MAX_BUF + 1, "(%.*s, %.*s)",
                  utf8_nbytes(interval->lbound), interval->lbound,
                  utf8_nbytes(interval->ubound), interval->ubound);

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

/* --- Regex ---------------------------------------------------------------- */

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

Regex *regex_cc(Interval *intervals, len_t len)
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

Regex *regex_single_child(RegexType type, Regex *child, byte pos)
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

Regex *regex_counter(Regex *child, byte greedy, cntr_t min, cntr_t max)
{
    Regex *re = malloc(sizeof(Regex));

    re->type = COUNTER;
    re->left = child;
    re->pos  = greedy;
    re->min  = min;
    re->max  = max;

    return re;
}

char *regex_to_tree_str(Regex *re)
{
    size_t len = 0, alloc = BUF;

    char *s = malloc(alloc * sizeof(char));
    regex_to_tree_str_indent(s, &len, &alloc, re, 0);
    return s;
}

static void regex_to_tree_str_indent(char   *s,
                                     size_t *len,
                                     size_t *alloc,
                                     Regex  *re,
                                     int     indent)
{
    char *p;

    if (re == NULL) return;

    ENSURE_SPACE(s, *len + indent + 1, *alloc, sizeof(char));
    *len += snprintf(s + *len, *alloc - *len, "%*s", indent, "");

    switch (re->type) {
        case CARET: STR_PUSH(s, *len, *alloc, "Caret"); break;

        case DOLLAR: STR_PUSH(s, *len, *alloc, "Dollar"); break;

        case LITERAL:
            ENSURE_SPACE(s, *len + 14, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "Literal(%.*s)",
                             utf8_nbytes(re->ch), re->ch);
            break;

        case CC:
            p = intervals_to_str(re->intervals, re->cc_len);
            ENSURE_SPACE(s, *len + 12 + strlen(p), *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "CharClass(%s)", p);
            free(p);
            break;

        case ALT: STR_PUSH(s, *len, *alloc, "Alternation");
        case CONCAT:
            if (re->type == CONCAT) STR_PUSH(s, *len, *alloc, "Concatenation");
            if (re->left) {
                ENSURE_SPACE(s, *len + indent + 7, *alloc, sizeof(char));
                *len += snprintf(s + *len, *alloc - *len, "\n%*sleft:\n",
                                 indent, "");
                regex_to_tree_str_indent(s, len, alloc, re->left, indent + 2);
            }

            if (re->right) {
                ENSURE_SPACE(s, *len + indent + 8, *alloc, sizeof(char));
                *len += snprintf(s + *len, *alloc - *len, "\n%*sright:\n",
                                 indent, "");
                regex_to_tree_str_indent(s, len, alloc, re->right, indent + 2);
            }
            break;

        case CAPTURE: STR_PUSH(s, *len, *alloc, "Capture"); goto body;
        case STAR:
            ENSURE_SPACE(s, *len + 9, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "Star(%d)", re->pos);
            goto body;
        case PLUS:
            ENSURE_SPACE(s, *len + 9, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "Plus(%d)", re->pos);
            goto body;
        case QUES:
            ENSURE_SPACE(s, *len + 9, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "Ques(%d)", re->pos);
            goto body;
        case COUNTER:
            ENSURE_SPACE(s, *len + 55, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len,
                             "Counter(%d, " CNTR_FMT ", ", re->pos, re->min);
            if (re->max == CNTR_MAX) {
                *len += snprintf(s + *len, *alloc - *len, "inf)");
            } else {
                *len +=
                    snprintf(s + *len, *alloc - *len, CNTR_FMT ")", re->max);
            }
            goto body;
        case LOOKAHEAD:
            ENSURE_SPACE(s, *len + 14, *alloc, sizeof(char));
            *len += snprintf(s + *len, *alloc - *len, "Lookahead(%d)", re->pos);
        body:
            if (re->left) {
                ENSURE_SPACE(s, *len + indent + 7, *alloc, sizeof(char));
                *len += snprintf(s + *len, *alloc - *len, "\n%*sbody:\n",
                                 indent, "");
                regex_to_tree_str_indent(s, len, alloc, re->left, indent + 2);
            }
            break;
    }
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
