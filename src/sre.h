#ifndef SRE_H
#define SRE_H

#include <stddef.h>

#define STC_UTF_ENABLE_SHORT_NAMES
#include "stc/util/utf.h"

#include "types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    int         neg;
    const char *lbound;
    const char *ubound;
} Interval;

typedef enum {
    CARET,
    DOLLAR,
    MEMOISE,
    LITERAL,
    CC,
    ALT,
    CONCAT,
    CAPTURE,
    STAR,
    PLUS,
    QUES,
    COUNTER,
    LOOKAHEAD,
    NREGEXTYPES
} RegexType;

typedef struct regex Regex;

struct regex {
    RegexType type;

    union {
        const char *ch;
        Interval   *intervals;
        Regex      *left;
    };

    union {
        byte   pos;
        len_t  cc_len;
        len_t  capture_idx;
        Regex *right;
    };

    cntr_t min;
    cntr_t max;
};

#define IS_UNARY_OP(type)      ((type) == STAR || (type) == PLUS || \
                                (type) == QUES || (type) == COUNTER)
#define IS_BINARY_OP(type)     ((type) == CONCAT || (type) == ALT)
#define IS_OP(type)            (IS_BINARY_OP(type) || IS_UNARY_OP(type))
#define IS_PARENTHETICAL(type) ((type) == LOOKAHEAD || (type) == CAPTURE)
#define IS_TERMINAL(type)      (!(IS_OP(type) || IS_PARENTHETICAL(type)))

/* --- Interval function prototypes ----------------------------------------- */

#define interval_predicate(interval, codepoint)        \
    ((utf8_cmp((interval).lbound, (codepoint)) <= 0 && \
      utf8_cmp((codepoint), (interval).ubound) >= 0) != (interval).neg)

Interval interval(int neg, const char *lbound, const char *ubound);
char    *interval_to_str(Interval *self);

int intervals_predicate(Interval *intervals, size_t len, const char *codepoint);
char *intervals_to_str(Interval *intervals, size_t len);

/* --- Regex function prototypes -------------------------------------------- */

Regex *regex_anchor(RegexType type);
Regex *regex_literal(const char *ch);
Regex *regex_cc(Interval *intervals, len_t len);
Regex *regex_branch(RegexType kind, Regex *left, Regex *right);
Regex *regex_capture(Regex *child, len_t idx);
Regex *regex_single_child(RegexType kind, Regex *child, byte pos);
Regex *regex_counter(Regex *child, byte greedy, cntr_t min, cntr_t max);

void   regex_free(Regex *self);
Regex *regex_clone(const Regex *self);
char  *regex_to_tree_str(const Regex *self);

#endif /* SRE_H */
