#ifndef SRE_H
#define SRE_H

#include <stddef.h>

#include "types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct regex Regex;

typedef struct {
    int         neg;
    const char *lbound;
    const char *ubound;
} Interval;

typedef enum {
    CARET,
    DOLLAR,
    LITERAL,
    CC,
    ALT,
    CONCAT,
    CAPTURE,
    STAR,
    PLUS,
    QUES,
    COUNTER,
    LOOKAHEAD
} RegexType;

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
        Regex *right;
    };

    cntr_t min;
    cntr_t max;
};

/* --- Interval function prototypes ----------------------------------------- */

Interval interval(int neg, const char *lbound, const char *ubound);

char *interval_to_str(Interval *interval);
char *intervals_to_str(Interval *intervals, size_t len);

/* --- Regex function prototypes -------------------------------------------- */

Regex *regex_anchor(RegexType type);
Regex *regex_literal(const char *ch);
Regex *regex_cc(Interval *intervals, len_t len);
Regex *regex_branch(RegexType kind, Regex *left, Regex *right);
Regex *regex_single_child(RegexType kind, Regex *child, byte pos);
Regex *regex_counter(Regex *child, byte greedy, cntr_t min, cntr_t max);

char *regex_to_tree_str(Regex *re);
void  regex_free(Regex *re);

#endif /* SRE_H */
