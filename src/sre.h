#ifndef SRE_H
#define SRE_H

#include "types.h"

typedef struct regex_tree RegexTree;

typedef struct {
    int  neg;
    uint lbound;
    uint ubound;
} Interval;

typedef enum {
    CARET,
    DOLLAR,
    LITERAL,
    CHAR_CLASS,
    ALT,
    CONCAT,
    CAPTURE,
    STAR,
    PLUS,
    QUES,
    COUNTER,
    LOOKAHEAD
} RegexKind;

struct regex_tree {
    RegexKind kind;

    union {
        int        ch;
        Interval  *intervals;
        RegexTree *child;
    };

    union {
        int        pos;
        RegexTree *child_;
    };

    uint min;
    uint max;
};

Interval *interval(int neg, uint lbound, uint ubound);

RegexTree *regex_tree_anchor(RegexKind kind);
RegexTree *regex_tree_literal(int ch);
RegexTree *regex_tree_cc(Interval *intervals);
RegexTree *
regex_tree_branch(RegexKind kind, RegexTree *left, RegexTree *right);
RegexTree *regex_tree_single_child(RegexKind kind, RegexTree *child, int pos);
RegexTree *regex_tree_counter(RegexTree *child, int pos, uint min, uint max);

#endif /* SRE_H */
