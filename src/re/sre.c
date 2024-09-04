#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "../utils.h"
#include "sre.h"

#define BUF              512
#define INTERVAL_MAX_BUF 26

static void
regex_print_tree_indent(FILE *stream, const BruRegexNode *re, int indent);

/* --- BruInterval ---------------------------------------------------------- */

BruInterval bru_interval(const char *lbound, const char *ubound)
{
    return (BruInterval){ lbound, ubound };
}

char *bru_interval_to_str(const BruInterval *self)
{
    char  *s = malloc(INTERVAL_MAX_BUF * sizeof(*s));
    size_t codepoint, len = 0;

    codepoint = stc_utf8_to_codepoint(self->lbound);
    if (stc_unicode_isprint(codepoint))
        len += snprintf(s + len, INTERVAL_MAX_BUF - len, "(%.*s, ",
                        stc_utf8_nbytes(self->lbound), self->lbound);
    else
        len += snprintf(s + len, INTERVAL_MAX_BUF - len,
                        "(" STC_UNICODE_ESCAPE_FMT ", ",
                        STC_UNICODE_ESCAPE_ARG(codepoint));

    codepoint = stc_utf8_to_codepoint(self->ubound);
    if (stc_unicode_isprint(codepoint))
        snprintf(s + len, INTERVAL_MAX_BUF - len, "%.*s)",
                 stc_utf8_nbytes(self->ubound), self->ubound);
    else
        snprintf(s + len, INTERVAL_MAX_BUF - len, STC_UNICODE_ESCAPE_FMT ")",
                 STC_UNICODE_ESCAPE_ARG(codepoint));

    return s;
}

BruIntervals *bru_intervals_new(int neg, size_t len)
{
    BruIntervals *intervals =
        malloc(sizeof(*intervals) + len * sizeof(*intervals->intervals));

    intervals->neg = neg ? TRUE : FALSE;
    intervals->len = len;

    return intervals;
}

void bru_intervals_free(BruIntervals *self) { free(self); }

BruIntervals *bru_intervals_clone(const BruIntervals *self)
{
    BruIntervals *clone = bru_intervals_new(self->neg, self->len);

    memcpy(clone->intervals, self->intervals,
           self->len * sizeof(*self->intervals));

    return clone;
}

int bru_intervals_predicate(const BruIntervals *self, const char *ch)
{
    size_t i;
    int    pred = 0;

    for (i = 0; i < self->len; i++)
        if ((pred = bru_interval_predicate(self->intervals[i], ch))) break;

    return pred != self->neg;
}

char *bru_intervals_to_str(const BruIntervals *self)
{
    size_t i, slen = 0, alloc = 4 + 2 * INTERVAL_MAX_BUF;
    char  *s = malloc(alloc * sizeof(*s)), *p;

    if (self->neg) s[slen++] = '^';
    s[slen++] = '[';
    for (i = 0; i < self->len; ++i) {
        p = bru_interval_to_str(self->intervals + i);
        BRU_ENSURE_SPACE(s, slen + INTERVAL_MAX_BUF + 3, alloc, sizeof(char));
        if (i < self->len - 1)
            slen += snprintf(s + slen, alloc - slen, "%s, ", p);
        else
            slen += snprintf(s + slen, alloc - slen, "%s", p);
        free(p);
    }
    BRU_ENSURE_SPACE(s, slen + 2, alloc, sizeof(char));
    s[slen++] = ']';
    s[slen]   = '\0';

    return s;
}

/* --- BruRegex ------------------------------------------------------------- */

BruRegexNode *bru_regex_new(BruRegexType type)
{
    BruRegexNode *re = malloc(sizeof(*re));

    /* check `type` to make sure correct node type */
    assert(type == BRU_CARET || type == BRU_DOLLAR || /* type == MEMOISE || */
           type == BRU_EPSILON);
    re->type     = type;
    re->nullable = TRUE;

    return re;
}

BruRegexNode *bru_regex_literal(const char *ch)
{
    BruRegexNode *re = malloc(sizeof(*re));

    re->type     = BRU_LITERAL;
    re->ch       = ch;
    re->nullable = FALSE;

    return re;
}

BruRegexNode *bru_regex_cc(BruIntervals *intervals)
{
    BruRegexNode *re = malloc(sizeof(*re));

    re->type      = BRU_CC;
    re->intervals = intervals;
    re->nullable  = FALSE;

    return re;
}

BruRegexNode *
bru_regex_branch(BruRegexType type, BruRegexNode *left, BruRegexNode *right)
{
    BruRegexNode *re = malloc(sizeof(*re));

    /* check `type` to make sure correct node type */
    assert(type == BRU_ALT || type == BRU_CONCAT);
    re->type     = type;
    re->left     = left;
    re->right    = right;
    re->nullable = (type == BRU_ALT) ? left->nullable || right->nullable
                                     : left->nullable && right->nullable;

    return re;
}

BruRegexNode *bru_regex_capture(BruRegexNode *child, bru_len_t idx)
{
    BruRegexNode *re = malloc(sizeof(*re));

    re->type        = BRU_CAPTURE;
    re->left        = child;
    re->capture_idx = idx;
    re->nullable    = child->nullable;

    return re;
}

BruRegexNode *bru_regex_backreference(bru_len_t idx)
{
    BruRegexNode *re = malloc(sizeof(*re));

    re->type        = BRU_BACKREFERENCE;
    re->capture_idx = idx;
    assert(FALSE && "TODO: Are backreferences nullable?");

    return re;
}

BruRegexNode *
bru_regex_repetition(BruRegexType type, BruRegexNode *child, bru_byte_t greedy)
{
    BruRegexNode *re = malloc(sizeof(*re));

    /* check `type` to make sure correct node type */
    assert(type == BRU_STAR || type == BRU_PLUS || type == BRU_QUES);
    re->type     = type;
    re->left     = child;
    re->greedy   = greedy;
    re->nullable = (type == BRU_PLUS) ? child->nullable : TRUE;

    return re;
}

BruRegexNode *bru_regex_counter(BruRegexNode *child,
                                bru_byte_t    greedy,
                                bru_cntr_t    min,
                                bru_cntr_t    max)
{
    BruRegexNode *re = malloc(sizeof(*re));

    re->type     = BRU_COUNTER;
    re->left     = child;
    re->greedy   = greedy;
    re->min      = min;
    re->max      = max;
    re->nullable = min == 0 || child->nullable;

    return re;
}

BruRegexNode *bru_regex_lookahead(BruRegexNode *child, bru_byte_t positive)
{
    BruRegexNode *re = malloc(sizeof(*re));

    re->type     = BRU_LOOKAHEAD;
    re->left     = child;
    re->positive = positive;
    assert(FALSE && "TODO: Are lookaheads nullable?");

    return re;
}

void bru_regex_node_free(BruRegexNode *self)
{
    switch (self->type) {
        case BRU_EPSILON: /* fallthrough */
        case BRU_CARET:   /* fallthrough */
        case BRU_DOLLAR:  /* fallthrough */
        // case MEMOISE: /* fallthrough */
        case BRU_LITERAL: /* fallthrough */
        case BRU_BACKREFERENCE: break;

        case BRU_CC: bru_intervals_free(self->intervals); break;

        case BRU_ALT: /* fallthrough */
        case BRU_CONCAT:
            bru_regex_node_free(self->left);
            bru_regex_node_free(self->right);
            break;

        case BRU_CAPTURE: /* fallthrough */
        case BRU_STAR:    /* fallthrough */
        case BRU_PLUS:    /* fallthrough */
        case BRU_QUES:    /* fallthrough */
        case BRU_COUNTER: /* fallthrough */
        case BRU_LOOKAHEAD: bru_regex_node_free(self->left); break;
        case BRU_NREGEXTYPES: assert(0 && "unreachable");
    }

    free(self);
}

BruRegexNode *bru_regex_clone(const BruRegexNode *self)
{
    BruRegexNode *re = malloc(sizeof(*re));

    memcpy(re, self, sizeof(*re));
    switch (re->type) {
        case BRU_EPSILON: /* fallthrough */
        case BRU_CARET:   /* fallthrough */
        case BRU_DOLLAR:  /* fallthrough */
        // case BRU_MEMOISE: /* fallthrough */
        case BRU_LITERAL: /* fallthrough */
        case BRU_BACKREFERENCE: break;

        case BRU_CC:
            re->intervals = bru_intervals_clone(self->intervals);
            break;

        case BRU_ALT: /* fallthrough */
        case BRU_CONCAT:
            re->left  = bru_regex_clone(self->left);
            re->right = bru_regex_clone(self->right);
            break;

        case BRU_CAPTURE: /* fallthrough */
        case BRU_STAR:    /* fallthrough */
        case BRU_PLUS:    /* fallthrough */
        case BRU_QUES:    /* fallthrough */
        case BRU_COUNTER: /* fallthrough */
        case BRU_LOOKAHEAD: re->left = bru_regex_clone(self->left); break;
        case BRU_NREGEXTYPES: assert(0 && "unreachable");
    }

    return re;
}

void bru_regex_print_tree(const BruRegexNode *self, FILE *stream)
{
    regex_print_tree_indent(stream, self, 0);
    fputc('\n', stream);
}

static void
regex_print_tree_indent(FILE *stream, const BruRegexNode *re, int indent)
{
    char *p;

    if (re == NULL) return;

    fprintf(stream, "%*s", indent, "");
    fprintf(stream, "%06lu: ", re->rid);
    fputs(re->nullable ? "*" : "", stream);

    switch (re->type) {
        case BRU_EPSILON: fputs("Epsilon", stream); break;
        case BRU_CARET: fputs("Caret", stream); break;
        case BRU_DOLLAR: fputs("Dollar", stream); break;

        // case BRU_MEMOISE: fputs("Memoise", stream); break;
        case BRU_LITERAL:
            fprintf(stream, "Literal(%.*s)", stc_utf8_nbytes(re->ch), re->ch);
            break;

        case BRU_CC:
            p = bru_intervals_to_str(re->intervals);
            fprintf(stream, "CharClass(%s)", p);
            free(p);
            break;

        case BRU_ALT: fputs("Alternation", stream); goto concat_body;
        case BRU_CONCAT:
            fputs("Concatenation", stream);
        concat_body:
            if (re->left) {
                fprintf(stream, "\n%*sleft:\n", indent, "");
                regex_print_tree_indent(stream, re->left, indent + 2);
            }

            if (re->right) {
                fprintf(stream, "\n%*sright:\n", indent, "");
                regex_print_tree_indent(stream, re->right, indent + 2);
            }
            break;

        case BRU_CAPTURE:
            fprintf(stream, "Capture(%d)", re->capture_idx);
            goto body;
        case BRU_STAR: fprintf(stream, "Star(%d)", re->greedy); goto body;
        case BRU_PLUS: fprintf(stream, "Plus(%d)", re->greedy); goto body;
        case BRU_QUES: fprintf(stream, "Ques(%d)", re->greedy); goto body;
        case BRU_COUNTER:
            fprintf(stream, "Counter(%d, " BRU_CNTR_FMT ", ", re->greedy,
                    re->min);
            if (re->max < BRU_CNTR_MAX)
                fprintf(stream, BRU_CNTR_FMT ")", re->max);
            else
                fprintf(stream, "inf)");
            goto body;
        case BRU_LOOKAHEAD:
            fprintf(stream, "Lookahead(%d)", re->positive);
        body:
            if (re->left) {
                fprintf(stream, "\n%*sbody:\n", indent, "");
                regex_print_tree_indent(stream, re->left, indent + 2);
            }
            break;

        case BRU_BACKREFERENCE:
            fprintf(stream, "Backreference(%d)", re->capture_idx);
            break;

        case BRU_NREGEXTYPES: assert(0 && "unreachable");
    }
}
