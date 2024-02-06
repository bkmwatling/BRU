#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stc/util/utf.h"

#include "sre.h"
#include "utils.h"

#define BUF              512
#define INTERVAL_MAX_BUF 13

static void regex_to_tree_str_indent(char           **s,
                                     size_t          *len,
                                     size_t          *alloc,
                                     const RegexNode *re,
                                     int              indent);

/* --- Interval ------------------------------------------------------------- */

Interval interval(int neg, const char *lbound, const char *ubound)
{
    Interval interval = { neg, lbound, ubound };
    return interval;
}

char *interval_to_str(const Interval *self)
{
    char *s = malloc((INTERVAL_MAX_BUF + 1) * sizeof(char)), *p = s;

    if (self->neg) *p++ = '^';
    p += snprintf(p, INTERVAL_MAX_BUF + 1, "(%.*s, %.*s)",
                  stc_utf8_nbytes(self->lbound), self->lbound,
                  stc_utf8_nbytes(self->ubound), self->ubound);

    return s;
}

Interval *intervals_clone(const Interval *intervals, size_t len)
{
    Interval *clone;

    clone = malloc(sizeof(*clone) * len);
    memcpy(clone, intervals, sizeof(*clone) * len);

    return clone;
}

int intervals_predicate(const Interval *intervals,
                        size_t          len,
                        const char     *codepoint)
{
    size_t i;
    int    pred = 0;

    for (i = 0; i < len; i++)
        if ((pred = interval_predicate(intervals[i], codepoint))) break;

    return pred;
}

char *intervals_to_str(const Interval *intervals, size_t len)
{
    size_t i, slen = 0, alloc = 3 + 2 * INTERVAL_MAX_BUF;
    char  *s = malloc(alloc * sizeof(char)), *p;

    s[slen++] = '[';
    for (i = 0; i < len; ++i) {
        p = interval_to_str(intervals + i);
        ENSURE_SPACE(s, slen + INTERVAL_MAX_BUF + 3, alloc, sizeof(char));
        if (i < len - 1)
            slen += snprintf(s + slen, alloc - slen, "%s, ", p);
        else
            slen += snprintf(s + slen, alloc - slen, "%s", p);
        free(p);
    }
    ENSURE_SPACE(s, slen + 2, alloc, sizeof(char));
    s[slen++] = ']';
    s[slen]   = '\0';

    return s;
}

/* --- Regex ---------------------------------------------------------------- */

RegexNode *regex_new(RegexType type)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    /* check `type` to make sure correct node type */
    assert(type == CARET || type == DOLLAR || type == MEMOISE ||
           type == EPSILON);
    re->type = type;

    return re;
}

RegexNode *regex_literal(const char *ch)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    re->type = LITERAL;
    re->ch   = ch;

    return re;
}

RegexNode *regex_cc(Interval *intervals, len_t len)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    re->type      = CC;
    re->intervals = intervals;
    re->cc_len    = len;

    return re;
}

RegexNode *regex_branch(RegexType type, RegexNode *left, RegexNode *right)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    /* check `type` to make sure correct node type */
    assert(type == ALT || type == CONCAT);
    re->type  = type;
    re->left  = left;
    re->right = right;

    return re;
}

RegexNode *regex_capture(RegexNode *child, len_t idx)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    re->type        = CAPTURE;
    re->left        = child;
    re->capture_idx = idx;

    return re;
}

RegexNode *regex_backreference(len_t idx)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    re->type        = BACKREFERENCE;
    re->capture_idx = idx;

    return re;
}

RegexNode *regex_single_child(RegexType type, RegexNode *child, byte pos)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    /* check `type` to make sure correct node type */
    assert(type == STAR || type == PLUS || type == QUES || type == LOOKAHEAD);
    re->type = type;
    re->left = child;
    re->pos  = pos;

    return re;
}

RegexNode *regex_counter(RegexNode *child, byte greedy, cntr_t min, cntr_t max)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    re->type = COUNTER;
    re->left = child;
    re->pos  = greedy;
    re->min  = min;
    re->max  = max;

    return re;
}

void regex_node_free(RegexNode *self)
{
    switch (self->type) {
        case EPSILON: /* fallthrough */
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case MEMOISE: /* fallthrough */
        case LITERAL: /* fallthrough */
        case BACKREFERENCE: break;

        case CC: free(self->intervals); break;

        case ALT: /* fallthrough */
        case CONCAT:
            regex_node_free(self->left);
            regex_node_free(self->right);
            break;

        case CAPTURE: /* fallthrough */
        case STAR:    /* fallthrough */
        case PLUS:    /* fallthrough */
        case QUES:    /* fallthrough */
        case COUNTER: /* fallthrough */
        case LOOKAHEAD: regex_node_free(self->left); break;
        case NREGEXTYPES: assert(0 && "unreachable");
    }

    free(self);
}

RegexNode *regex_clone(RegexNode *self)
{
    RegexNode *re = malloc(sizeof(RegexNode));

    memcpy(re, self, sizeof(RegexNode));
    switch (re->type) {
        case EPSILON: /* fallthrough */
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case MEMOISE: /* fallthrough */
        case LITERAL: /* fallthrough */
        case BACKREFERENCE: break;

        case CC:
            re->intervals = malloc(re->cc_len * sizeof(Interval));
            memcpy(re->intervals, self->intervals,
                   re->cc_len * sizeof(Interval));
            break;

        case ALT: /* fallthrough */
        case CONCAT:
            re->left  = regex_clone(self->left);
            re->right = regex_clone(self->right);
            break;

        case CAPTURE: /* fallthrough */
        case STAR:    /* fallthrough */
        case PLUS:    /* fallthrough */
        case QUES:    /* fallthrough */
        case COUNTER: /* fallthrough */
        case LOOKAHEAD: re->left = regex_clone(self->left); break;
        case NREGEXTYPES: assert(0 && "unreachable");
    }

    return re;
}

char *regex_to_tree_str(const RegexNode *self)
{
    size_t len = 0, alloc = BUF;

    char *s = malloc(alloc * sizeof(char));
    regex_to_tree_str_indent(&s, &len, &alloc, self, 0);

    return s;
}

static void regex_to_tree_str_indent(char           **s,
                                     size_t          *len,
                                     size_t          *alloc,
                                     const RegexNode *re,
                                     int              indent)
{
    char *p;

    if (re == NULL) return;

    ENSURE_SPACE(*s, *len + indent + 1, *alloc, sizeof(char));
    *len += snprintf(*s + *len, *alloc - *len, "%*s", indent, "");

    ENSURE_SPACE(*s, *len + 9, *alloc, sizeof(char));
    *len += snprintf(*s + *len, *alloc - *len, "%06lu: ", re->rid);

    switch (re->type) {
        case EPSILON: STR_PUSH(*s, *len, *alloc, "Epsilon"); break;

        case CARET: STR_PUSH(*s, *len, *alloc, "Caret"); break;

        case DOLLAR: STR_PUSH(*s, *len, *alloc, "Dollar"); break;

        case MEMOISE: STR_PUSH(*s, *len, *alloc, "Memoise"); break;

        case LITERAL:
            ENSURE_SPACE(*s, *len + 14, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "Literal(%.*s)",
                             stc_utf8_nbytes(re->ch), re->ch);
            break;

        case CC:
            p = intervals_to_str(re->intervals, re->cc_len);
            ENSURE_SPACE(*s, *len + 12 + strlen(p), *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "CharClass(%s)", p);
            free(p);
            break;

        case ALT: STR_PUSH(*s, *len, *alloc, "Alternation"); goto concat_body;
        case CONCAT:
            STR_PUSH(*s, *len, *alloc, "Concatenation");
        concat_body:
            if (re->left) {
                ENSURE_SPACE(*s, *len + indent + 7, *alloc, sizeof(char));
                *len += snprintf(*s + *len, *alloc - *len, "\n%*sleft:\n",
                                 indent, "");
                regex_to_tree_str_indent(s, len, alloc, re->left, indent + 2);
            }

            if (re->right) {
                ENSURE_SPACE(*s, *len + indent + 8, *alloc, sizeof(char));
                *len += snprintf(*s + *len, *alloc - *len, "\n%*sright:\n",
                                 indent, "");
                regex_to_tree_str_indent(s, len, alloc, re->right, indent + 2);
            }
            break;

        case CAPTURE:
            ENSURE_SPACE(*s, *len + 15, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "Capture(%d)",
                             re->capture_idx);
            goto body;
        case STAR:
            ENSURE_SPACE(*s, *len + 10, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "Star(%d)", re->pos);
            goto body;
        case PLUS:
            ENSURE_SPACE(*s, *len + 10, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "Plus(%d)", re->pos);
            goto body;
        case QUES:
            ENSURE_SPACE(*s, *len + 10, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "Ques(%d)", re->pos);
            goto body;
        case COUNTER:
            ENSURE_SPACE(*s, *len + 56, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len,
                             "Counter(%d, " CNTR_FMT ", ", re->pos, re->min);
            if (re->max == CNTR_MAX) {
                *len += snprintf(*s + *len, *alloc - *len, "inf)");
            } else {
                *len +=
                    snprintf(*s + *len, *alloc - *len, CNTR_FMT ")", re->max);
            }
            goto body;
        case LOOKAHEAD:
            ENSURE_SPACE(*s, *len + 15, *alloc, sizeof(char));
            *len +=
                snprintf(*s + *len, *alloc - *len, "Lookahead(%d)", re->pos);
        body:
            if (re->left) {
                ENSURE_SPACE(*s, *len + indent + 7, *alloc, sizeof(char));
                *len += snprintf(*s + *len, *alloc - *len, "\n%*sbody:\n",
                                 indent, "");
                regex_to_tree_str_indent(s, len, alloc, re->left, indent + 2);
            }
            break;

        case BACKREFERENCE:
            ENSURE_SPACE(*s, *len + 17, *alloc, sizeof(char));
            *len += snprintf(*s + *len, *alloc - *len, "Backreference(%d)",
                             re->capture_idx);
            break;

        case NREGEXTYPES: assert(0 && "unreachable");
    }
}
