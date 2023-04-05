#ifndef SRE_H
#define SRE_H

typedef unsigned int      uint;
typedef struct regex_tree RegexTree;

typedef struct {
    int neg;
    int lbound;
    int ubound;
} Interval;

typedef enum {
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
        int        greedy;
        RegexTree *child_;
    };

    int min;
    int max;
};

#endif /* SRE_H */
