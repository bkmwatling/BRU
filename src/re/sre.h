#ifndef BRU_RE_SRE_H
#define BRU_RE_SRE_H

#include <stddef.h>
#include <stdio.h>

#include "../stc/util/utf.h"

#include "../types.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    const char *lbound; /**< lower bound for interval                         */
    const char *ubound; /**< upper bound for interval                         */
} BruInterval;

typedef struct {
    int         neg;         /**< whether to negate the intervals             */
    size_t      len;         /**< number of intervals                         */
    BruInterval intervals[]; /**< array of underlying intervals               */
} BruIntervals;

typedef enum {
    BRU_EPSILON,
    BRU_CARET,
    BRU_DOLLAR,
    // BRU_MEMOISE,
    BRU_LITERAL,
    BRU_CC,
    BRU_ALT,
    BRU_CONCAT,
    BRU_CAPTURE,
    BRU_STAR,
    BRU_PLUS,
    BRU_QUES,
    BRU_COUNTER,
    BRU_LOOKAHEAD,
    BRU_BACKREFERENCE,
    BRU_NREGEXTYPES
} BruRegexType;

typedef struct bru_regex_node BruRegexNode;
typedef size_t                bru_regex_id;

typedef struct {
    const char *regex;  /**< the regex string                                 */
    BruRegexNode *root; /**< the root node of the regex tree                  */
} BruRegex;

struct bru_regex_node {
    bru_regex_id rid;  /**< the unique identifier for this regex node         */
    BruRegexType type; /**< the type of the regex node                        */

    union {
        const char   *ch;        /**< UTF-8 encoded "character" for literals  */
        BruIntervals *intervals; /**< intervals for character classes         */
        BruRegexNode *left;      /**< left or only child for operators        */
    };

    union {
        bru_byte_t greedy;      /**< whether repetition operator is greedy    */
        bru_byte_t positive;    /**< whether lookahead is positive/negative   */
        bru_len_t  capture_idx; /**< index for captures and backreferences    */
        BruRegexNode *right;    /**< right child for binary operators         */
    };

    bru_cntr_t min;      /**< minimum value for counter                       */
    bru_cntr_t max;      /**< maximum value for counter                       */
    bru_byte_t nullable; /**< whether subtree at this node matches epsilon    */
};

#if !defined(BRU_RE_SRE_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_SRE_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&    \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) || \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef BruInterval  Interval;
typedef BruIntervals Intervals;

typedef BruRegexType RegexType;
#    define EPSILON       BRU_EPSILON
#    define CARET         BRU_CARET
#    define DOLLAR        BRU_DOLLAR
// #    define MEMOISE       BRU_MEMOISE
#    define LITERAL       BRU_LITERAL
#    define CC            BRU_CC
#    define ALT           BRU_ALT
#    define CONCAT        BRU_CONCAT
#    define CAPTURE       BRU_CAPTURE
#    define STAR          BRU_STAR
#    define PLUS          BRU_PLUS
#    define QUES          BRU_QUES
#    define COUNTER       BRU_COUNTER
#    define LOOKAHEAD     BRU_LOOKAHEAD
#    define BACKREFERENCE BRU_BACKREFERENCE
#    define NREGEXTYPES   BRU_NREGEXTYPES

typedef BruRegexNode RegexNode;
typedef bru_regex_id regex_id;
typedef BruRegex     Regex;
typedef BruRegexNode RegexNode;

#    define IS_UNARY_OP      BRU_IS_UNARY_OP
#    define IS_BINARY_OP     BRU_IS_BINARY_OP
#    define IS_OP            BRU_IS_OP
#    define IS_PARENTHETICAL BRU_IS_PARENTHETICAL
#    define IS_TERMINAL      BRU_IS_TERMINAL

#    define interval_predicate  bru_interval_predicate
#    define interval            bru_interval
#    define interval_to_str     bru_interval_to_str
#    define intervals_new       bru_intervals_new
#    define intervals_free      bru_intervals_free
#    define intervals_clone     bru_intervals_clone
#    define intervals_predicate bru_intervals_predicate
#    define intervals_to_str    bru_intervals_to_str

#    define regex_new           bru_regex_new
#    define regex_literal       bru_regex_literal
#    define regex_cc            bru_regex_cc
#    define regex_branch        bru_regex_branch
#    define regex_capture       bru_regex_capture
#    define regex_backreference bru_regex_backreference
#    define regex_repetition    bru_regex_repetition
#    define regex_counter       bru_regex_counter
#    define regex_lookahead     bru_regex_lookahead
#    define regex_node_free     bru_regex_node_free
#    define regex_clone         bru_regex_clone
#    define regex_print_tree    bru_regex_print_tree
#endif /* BRU_RE_SRE_ENABLE_SHORT_NAMES */

#define BRU_IS_UNARY_OP(type)                                          \
    ((type) == BRU_STAR || (type) == BRU_PLUS || (type) == BRU_QUES || \
     (type) == BRU_COUNTER)
#define BRU_IS_BINARY_OP(type) ((type) == BRU_CONCAT || (type) == BRU_ALT)
#define BRU_IS_OP(type)        (BRU_IS_BINARY_OP(type) || BRU_IS_UNARY_OP(type))
#define BRU_IS_PARENTHETICAL(type) \
    ((type) == BRU_LOOKAHEAD || (type) == BRU_CAPTURE)
#define BRU_IS_TERMINAL(type) (!(BRU_IS_OP(type) || BRU_IS_PARENTHETICAL(type)))

/* --- BruInterval function prototypes -------------------------------------- */

#define bru_interval_predicate(interval, ch)       \
    (stc_utf8_cmp((interval).lbound, (ch)) <= 0 && \
     stc_utf8_cmp((ch), (interval).ubound) <= 0)

/**
 * Construct an interval from bounds.
 *
 * @param[in] lbound the lower bound of the interval
 * @param[in] ubound the upper bound of the interval
 *
 * @return the constructed interval
 */
BruInterval bru_interval(const char *lbound, const char *ubound);

/**
 * Get the string representation of an interval.
 *
 * @param[in] self the interval
 *
 * @return the string representation of the interval
 */
char *bru_interval_to_str(const BruInterval *self);

/**
 * Construct a collection of empty intervals with specified number of intervals
 * to allocate.
 *
 * @param[in] neg whether the collection of intervals should be negated
 * @param[in] len number of intervals to allocate space for
 *
 * @return a collection of intervals with specified number of intervals
 *         allocated and specified negation
 */
BruIntervals *bru_intervals_new(int neg, size_t len);

/**
 * Free the memory allocated for the collection of intervals.
 *
 * @param[in] self the collection of intervals to free
 */
void bru_intervals_free(BruIntervals *self);

/**
 * Clone a collection of intervals.
 *
 * @param[in] self the collection of intervals to clone
 *
 * @return the cloned collection of intervals
 */
BruIntervals *bru_intervals_clone(const BruIntervals *self);

/**
 * Evaluate the predicate represented by a collection of intervals.
 *
 * @param[in] self the collection of intervals to evaluate
 * @param[in] ch   the UTF-8 encoded "character" to evaluate against
 *
 * @return truthy value if the UTF-8 "character" is contained in the
 *         collection of intervals; else 0
 */
int bru_intervals_predicate(const BruIntervals *self, const char *ch);

/**
 * Get the string representation of a collection of intervals.
 *
 * @param[in] self the collection of intervals
 *
 * @return the string representation of the collection of intervals
 */
char *bru_intervals_to_str(const BruIntervals *self);

/* --- BruRegex function prototypes ----------------------------------------- */

/**
 * Construct a base regex node with given type.
 *
 * @param[in] type the type for the regex node
 *
 * @return the regex node with given type
 */
BruRegexNode *bru_regex_new(BruRegexType type);

/**
 * Construct a regex node with type LITERAL with given literal codepoint.
 *
 * @param[in] ch the UTF-8 encoded codepoint
 *
 * @return the regex node with literal codepoint
 */
BruRegexNode *bru_regex_literal(const char *ch);

/**
 * Construct a regex character class node.
 *
 * @param[in] intervals the collection of intervals for the character class
 *
 * @return the regex character class node
 */
BruRegexNode *bru_regex_cc(BruIntervals *intervals);

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
BruRegexNode *
bru_regex_branch(BruRegexType type, BruRegexNode *left, BruRegexNode *right);

/**
 * Construct a regex capture node with single child and capture index.
 *
 * @param[in] child the regex tree contained in the capture
 * @param[in] idx   the index of the capture
 *
 * @return the constructed regex capture node
 */
BruRegexNode *bru_regex_capture(BruRegexNode *child, bru_len_t idx);

/**
 * Construct a regex backreference node with capture index it matches against.
 *
 * @param[in] idx the capture index the backreference refers to
 *
 * @return the constructed regex backreference node
 */
BruRegexNode *bru_regex_backreference(bru_len_t idx);

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
BruRegexNode *
bru_regex_repetition(BruRegexType type, BruRegexNode *child, bru_byte_t greedy);

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
BruRegexNode *bru_regex_counter(BruRegexNode *child,
                                bru_byte_t    greedy,
                                bru_cntr_t    min,
                                bru_cntr_t    max);

/**
 * Construct a regex lookahead node with given child and whether it is a
 * positive or negative lookahead.
 *
 * @param[in] child the regex tree child contained in the lookahead
 * @param[in] pos   whether the lookahead is positive or negative
 *
 * @return the constructed regex lookahead node
 */
BruRegexNode *bru_regex_lookahead(BruRegexNode *child, bru_byte_t pos);

/**
 * Free the memory allocated for the regex tree (the regex node and it's
 * possible children).
 *
 * @param[in] self the regex node at the root of the regex tree
 */
void bru_regex_node_free(BruRegexNode *self);

/**
 * Clone the regex tree from the given root node.
 *
 * @param[in] self the root node of the regex tree to clone
 *
 * @return the root of the cloned regex tree
 */
BruRegexNode *bru_regex_clone(const BruRegexNode *self);

/**
 * Print the tree representation of a regex tree to given file stream.
 *
 * @param[in] self   the root node of the regex tree
 * @param[in] stream the file stream to print to
 */
void bru_regex_print_tree(const BruRegexNode *self, FILE *stream);

#endif /* BRU_RE_SRE_H */
