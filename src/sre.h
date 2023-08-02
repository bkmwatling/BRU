#ifndef SRE_H
#define SRE_H

#include "stddef.h"
#include "types.h"

typedef struct regex_tree RegexTree;

typedef struct {
    int         neg;
    const char *lbound;
    const char *ubound;
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
        const char *ch;
        Interval   *intervals;
        RegexTree  *child;
    };

    union {
        int        pos;
        size_t     cc_len;
        RegexTree *child_;
    };

    uint min;
    uint max;
};

Interval interval(int neg, const char *lbound, const char *ubound);

char *interval_to_str(Interval *interval);
char *intervals_to_str(Interval *intervals, size_t len);

RegexTree *regex_tree_anchor(RegexKind kind);
RegexTree *regex_tree_literal(const char *ch);
RegexTree *regex_tree_cc(Interval *intervals, size_t len);
RegexTree *regex_tree_branch(RegexKind kind, RegexTree *left, RegexTree *right);
RegexTree *regex_tree_single_child(RegexKind kind, RegexTree *child, int pos);
RegexTree *regex_tree_counter(RegexTree *child, int greedy, uint min, uint max);

char *regex_tree_to_tree_str(RegexTree *re_tree);
void  regex_tree_free(RegexTree *re_tree);

#endif /* SRE_H */
