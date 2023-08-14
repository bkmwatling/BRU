#ifndef SRE_H
#define SRE_H

#include "stddef.h"
#include "types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct regex_s Regex;

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

struct regex_s {
    RegexType type;

    union {
        const char *ch;
        Interval   *intervals;
        Regex      *left;
    };

    union {
        int    pos;
        size_t cc_len;
        Regex *right;
    };

    uint min;
    uint max;
};

/* --- Interval function prototypes ----------------------------------------- */

Interval interval(int neg, const char *lbound, const char *ubound);

char *interval_to_str(Interval *interval);
char *intervals_to_str(Interval *intervals, size_t len);

/* --- RegexTree function prototypes ---------------------------------------- */

Regex *regex_anchor(RegexType kind);
Regex *regex_literal(const char *ch);
Regex *regex_cc(Interval *intervals, size_t len);
Regex *regex_branch(RegexType kind, Regex *left, Regex *right);
Regex *regex_single_child(RegexType kind, Regex *child, int pos);
Regex *regex_counter(Regex *child, int greedy, uint min, uint max);

char *regex_to_tree_str(Regex *re);
void  regex_free(Regex *re);

#endif /* SRE_H */
