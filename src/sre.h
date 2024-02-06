#ifndef SRE_H
#define SRE_H

#include <stddef.h>

#include "stc/util/utf.h"

#include "types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    int         neg;
    const char *lbound;
    const char *ubound;
} Interval;

typedef enum {
    EPSILON,
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
    BACKREFERENCE,
    NREGEXTYPES
} RegexType;

typedef struct regex_node RegexNode;
typedef size_t            regex_id;

typedef struct {
    const char *regex;
    RegexNode  *root;
} Regex;

struct regex_node {
    regex_id  rid;
    RegexType type;

    union {
        const char *ch;
        Interval   *intervals;
        RegexNode  *left;
    };

    union {
        byte       pos;
        len_t      cc_len;
        len_t      capture_idx;
        RegexNode *right;
    };

    cntr_t min;
    cntr_t max;
};

#define IS_UNARY_OP(type) \
    ((type) == STAR || (type) == PLUS || (type) == QUES || (type) == COUNTER)
#define IS_BINARY_OP(type)     ((type) == CONCAT || (type) == ALT)
#define IS_OP(type)            (IS_BINARY_OP(type) || IS_UNARY_OP(type))
#define IS_PARENTHETICAL(type) ((type) == LOOKAHEAD || (type) == CAPTURE)
#define IS_TERMINAL(type)      (!(IS_OP(type) || IS_PARENTHETICAL(type)))

/* --- Interval function prototypes ----------------------------------------- */

#define interval_predicate(interval, codepoint)            \
    ((stc_utf8_cmp((interval).lbound, (codepoint)) <= 0 && \
      stc_utf8_cmp((codepoint), (interval).ubound) <= 0) != (interval).neg)

Interval interval(int neg, const char *lbound, const char *ubound);
char    *interval_to_str(const Interval *self);

Interval *intervals_clone(const Interval *intervals, size_t len);
int       intervals_predicate(const Interval *intervals,
                              size_t          len,
                              const char     *codepoint);
char     *intervals_to_str(const Interval *intervals, size_t len);

/* --- Regex function prototypes -------------------------------------------- */

RegexNode *regex_new(RegexType type);
RegexNode *regex_literal(const char *ch);
RegexNode *regex_cc(Interval *intervals, len_t len);
RegexNode *regex_branch(RegexType kind, RegexNode *left, RegexNode *right);
RegexNode *regex_capture(RegexNode *child, len_t idx);
RegexNode *regex_backreference(len_t idx);
RegexNode *regex_single_child(RegexType kind, RegexNode *child, byte pos);
RegexNode *regex_counter(RegexNode *child, byte greedy, cntr_t min, cntr_t max);

void       regex_node_free(RegexNode *self);
RegexNode *regex_clone(RegexNode *self);
char      *regex_to_tree_str(const RegexNode *self);

#endif /* SRE_H */
