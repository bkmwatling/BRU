#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "stc/util/utf.h"

#include "sre.h"
#include "utils.h"

#define BUF              512
#define INTERVAL_MAX_BUF 26

static void
regex_print_tree_indent(FILE *stream, const RegexNode *re, int indent);

/* --- Interval ------------------------------------------------------------- */

static uint utf8_str_to_num(const char *codepoint)
{
    short i, nbytes;
    uint  num = 0;

    for (i = 0, nbytes = stc_utf8_nbytes(codepoint); i < nbytes; i++)
        num += (uint) codepoint[i] << (8 * (nbytes - i - 1));

    return num;
}

static int utf8_isprint(const char *codepoint)
{
    /* TODO: check for more than just ASCII isprint */
    return *codepoint > 127 || isprint(*codepoint);
}

Interval interval(int neg, const char *lbound, const char *ubound)
{
    return (Interval){ neg, lbound, ubound };
}

char *interval_to_str(const Interval *self)
{
    char  *s   = malloc(INTERVAL_MAX_BUF * sizeof(*s));
    size_t len = 0;

    if (self->neg) s[len++] = '^';
    if (utf8_isprint(self->lbound))
        len += snprintf(s + len, INTERVAL_MAX_BUF - len, "(%.*s, ",
                        stc_utf8_nbytes(self->lbound), self->lbound);
    else
        len += snprintf(s + len, INTERVAL_MAX_BUF - len, "(\\u%0*x, ",
                        2 * stc_utf8_nbytes(self->lbound),
                        utf8_str_to_num(self->lbound));

    if (utf8_isprint(self->ubound))
        snprintf(s + len, INTERVAL_MAX_BUF - len, "%.*s)",
                 stc_utf8_nbytes(self->ubound), self->ubound);
    else
        snprintf(s + len, INTERVAL_MAX_BUF - len, "\\u%0*x)",
                 2 * stc_utf8_nbytes(self->ubound),
                 utf8_str_to_num(self->ubound));

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
    char  *s = malloc(alloc * sizeof(*s)), *p;

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
    RegexNode *re = malloc(sizeof(*re));

    memcpy(re, self, sizeof(*re));
    switch (re->type) {
        case EPSILON: /* fallthrough */
        case CARET:   /* fallthrough */
        case DOLLAR:  /* fallthrough */
        case MEMOISE: /* fallthrough */
        case LITERAL: /* fallthrough */
        case BACKREFERENCE: break;

        case CC:
            re->intervals = malloc(re->cc_len * sizeof(*re->intervals));
            memcpy(re->intervals, self->intervals,
                   re->cc_len * sizeof(*re->intervals));
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

void regex_print_tree(const RegexNode *self, FILE *stream)
{
    regex_print_tree_indent(stream, self, 0);
    fputc('\n', stream);
}

static void
regex_print_tree_indent(FILE *stream, const RegexNode *re, int indent)
{
    char *p;

    if (re == NULL) return;

    fprintf(stream, "%*s", indent, "");
    fprintf(stream, "%06lu: ", re->rid);

    switch (re->type) {
        case EPSILON: fputs("Epsilon", stream); break;
        case CARET: fputs("Caret", stream); break;
        case DOLLAR: fputs("Dollar", stream); break;
        case MEMOISE: fputs("Memoise", stream); break;

        case LITERAL:
            fprintf(stream, "Literal(%.*s)", stc_utf8_nbytes(re->ch), re->ch);
            break;

        case CC:
            p = intervals_to_str(re->intervals, re->cc_len);
            fprintf(stream, "CharClass(%s)", p);
            free(p);
            break;

        case ALT: fputs("Alternation", stream); goto concat_body;
        case CONCAT:
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

        case CAPTURE:
            fprintf(stream, "Capture(%d)", re->capture_idx);
            goto body;
        case STAR: fprintf(stream, "Star(%d)", re->pos); goto body;
        case PLUS: fprintf(stream, "Plus(%d)", re->pos); goto body;
        case QUES: fprintf(stream, "Ques(%d)", re->pos); goto body;
        case COUNTER:
            fprintf(stream, "Counter(%d, " CNTR_FMT ", ", re->pos, re->min);
            if (re->max < CNTR_MAX)
                fprintf(stream, CNTR_FMT ")", re->max);
            else
                fprintf(stream, "inf)");
            goto body;
        case LOOKAHEAD:
            fprintf(stream, "Lookahead(%d)", re->pos);
        body:
            if (re->left) {
                fprintf(stream, "\n%*sbody:\n", indent, "");
                regex_print_tree_indent(stream, re->left, indent + 2);
            }
            break;

        case BACKREFERENCE:
            fprintf(stream, "Backreference(%d)", re->capture_idx);
            break;

        case NREGEXTYPES: assert(0 && "unreachable");
    }
}
