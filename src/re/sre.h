#ifndef SRE_H
#define SRE_H

#include <stddef.h>
#include <stdio.h>

#include "../stc/util/utf.h"

#include "../types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    int         neg;    /**< whether to negate interval                       */
    const char *lbound; /**< lower bound for interval                         */
    const char *ubound; /**< upper bound for interval                         */
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
    const char *regex; /**< the regex string                                  */
    RegexNode  *root;  /**< the root node of the regex tree                   */
} Regex;

struct regex_node {
    regex_id  rid;  /**< the unique identifier for this regex node            */
    RegexType type; /**< the type of the regex node                           */

    union {
        const char *ch;        /**< UTF-8 encoded codepoint for literals      */
        Interval   *intervals; /**< array of intervals for character classes  */
        RegexNode  *left;      /**< left or only child for operators          */
    };

    union {
        byte       greedy;   /**< whether the repetition operator is greedy   */
        byte       positive; /**< whether lookahead is positive or negative   */
        len_t      cc_len;   /**< number of intervals in character class      */
        len_t      capture_idx; /**< index for captures and backreferences    */
        RegexNode *right;       /**< right child for binary operators         */
    };

    cntr_t min; /**< minimum value for counter                                */
    cntr_t max; /**< maximum value for counter                                */
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

/**
 * Construct an interval from bounds and negation.
 *
 * @param[in] neg    whether the interval should be negated
 * @param[in] lbound the lower bound of the interval
 * @param[in] ubound the upper bound of the interval
 *
 * @return the constructed interval
 */
Interval interval(int neg, const char *lbound, const char *ubound);

/**
 * Get the string representation of an interval.
 *
 * @param[in] self the interval
 *
 * @return the string representation of the interval
 */
char *interval_to_str(const Interval *self);

/**
 * Clone an array of intervals.
 *
 * @param[in] intervals the array of intervals to clone
 * @param[in] len       the length of the array of intervals
 *
 * @return the cloned array of intervals
 */
Interval *intervals_clone(const Interval *intervals, size_t len);

/**
 * Evaluate the predicate represented by an array of intervals.
 *
 * @param[in] intervals the array of intervals to evaluate
 * @param[in] len       the length of the array of intervals
 * @param[in] codepoint the UTF-8 encoded codepoint to evaluate against
 *
 * @return whether the codepoint is contained in the array of intervals
 */
int intervals_predicate(const Interval *intervals,
                        size_t          len,
                        const char     *codepoint);

/**
 * Get the string representation of an array of intervals.
 *
 * @param[in] intervals the array of intervals
 * @param[in] len       the length of the array of intervals
 *
 * @return the string representation of the array of intervals
 */
char *intervals_to_str(const Interval *intervals, size_t len);

/* --- Regex function prototypes -------------------------------------------- */

/**
 * Construct a base regex node with given type.
 *
 * @param[in] type the type for the regex node
 *
 * @return the regex node with given type
 */
RegexNode *regex_new(RegexType type);

/**
 * Construct a regex node with type LITERAL with given literal codepoint.
 *
 * @param[in] ch the UTF-8 encoded codepoint
 *
 * @return the regex node with literal codepoint
 */
RegexNode *regex_literal(const char *ch);

/**
 * Construct a regex character class node.
 *
 * @param[in] intervals the intervals for the character class
 * @param[in] len       the number of intervals
 *
 * @return the regex character class node
 */
RegexNode *regex_cc(Interval *intervals, len_t len);

/**
 * Construct a regex branch node with given type (ALT or CONCAT) and given
 * children.
 *
 * @param[in] type  the type of the regex branch node (ALT or CONCAT)
 * @param[in] left  the left child of the branch
 * @param[in] right the right child of the branch
 *
 * @return the constructed regex branch node
 */
RegexNode *regex_branch(RegexType type, RegexNode *left, RegexNode *right);

/**
 * Construct a regex capture node with single child and capture index.
 *
 * @param[in] child the regex tree contained in the capture
 * @param[in] idx   the index of the capture
 *
 * @return the constructed regex capture node
 */
RegexNode *regex_capture(RegexNode *child, len_t idx);

/**
 * Construct a regex backreference node with capture index it matches against.
 *
 * @param[in] idx the capture index the backreference refers to
 *
 * @return the constructed regex backreference node
 */
RegexNode *regex_backreference(len_t idx);

/**
 * Construct a regex non-counter repetition node with given type and regex tree
 * child along with whether it is greedy.
 *
 * @param[in] type   the type of the repetition (STAR, PLUS, or QUES)
 * @param[in] child  the regex tree contained in the repetition
 * @param[in] greedy whether the repetition is greedy or lazy
 *
 * @return the constructed regex non-counter repetition node
 */
RegexNode *regex_repetition(RegexType type, RegexNode *child, byte greedy);

/**
 * Construct a regex counter node with given regex tree child, greediness, and
 * minimum and maximum counter values.
 *
 * @param[in] child  the regex tree child contained in the counter
 * @param[in] greedy whether the counter is greedy or lazy
 * @param[in] min    the minimum counter value
 * @param[in] max    the maximum counter value
 *
 * @return the constructed regex counter node
 */
RegexNode *regex_counter(RegexNode *child, byte greedy, cntr_t min, cntr_t max);

/**
 * Construct a regex lookahead node with given child and whether it is a
 * positive or negative lookahead.
 *
 * @param[in] child the regex tree child contained in the lookahead
 * @param[in] pos   whether the lookahead is positive or negative
 *
 * @return the constructed regex lookahead node
 */
RegexNode *regex_lookahead(RegexNode *child, byte pos);

/**
 * Free the memory allocated for the regex tree (the regex node and it's
 * possible children).
 *
 * @param[in] self the regex node at the root of the regex tree
 */
void regex_node_free(RegexNode *self);

/**
 * Clone the regex tree from the given root node.
 *
 * @param[in] self the root node of the regex tree to clone
 *
 * @return the root of the cloned regex tree
 */

/**
 * Print the tree representation of a regex tree to given file stream.
 *
 * @param[in] self   the root node of the regex tree
 * @param[in] stream the file stream to print to
 */
RegexNode *regex_clone(RegexNode *self);
void       regex_print_tree(const RegexNode *self, FILE *stream);

#endif /* SRE_H */
