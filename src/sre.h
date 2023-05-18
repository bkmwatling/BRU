#ifndef SRE_H
#define SRE_H

#include "types.h"
#include "utf8.h"

typedef struct regex_tree RegexTree;

typedef struct {
    int  neg;
    utf8 lbound;
    utf8 ubound;
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
        utf8       ch;
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

Interval *interval(int neg, utf8 lbound, utf8 ubound);

RegexTree *regex_tree_anchor(RegexKind kind);
RegexTree *regex_tree_literal(utf8 ch);
RegexTree *regex_tree_cc(Interval *intervals);
RegexTree *
regex_tree_branch(RegexKind kind, RegexTree *left, RegexTree *right);
RegexTree *regex_tree_single_child(RegexKind kind, RegexTree *child, int pos);
RegexTree *regex_tree_counter(RegexTree *child, int pos, uint min, uint max);

#endif /* SRE_H */
