/**
 * The Walker struct loosely mimics ANTLR4 visitors and listeners (in one).
 *
 * It maintains a map from Regex node types to walk() functions.
 * These functions control the exploration of the walker (visitor). Note the
 * Walker currently only supports DFS exploration; see TODO at bottom.
 * Altering the parse tree (in-place!) should happen in these functions.
 *
 * The default walker created when calling bru_walker_new() will walk the parse
 * tree in-order, but will trigger all events (pre-order, in-order,
 * post-order) where appropriate.
 *
 * The walk() functions also trigger events for listeners to act upon.
 * Currently supported events are pre-order traversal (i.e. before walking any
 * children), in-order traversal (i.e. after walking the first child), and
 * post-order traversal (i.e. after walking the last child).
 * Note on the default walker:
 *      If a node has 0 children, all three events are triggered consecutively.
 *      If a node has exactly 1 child, the pre-order traversal event is
 *      triggered, the child is walked, and the in-order and post-order events
 *      are triggered consecutively.
 *
 * Listener functions are provided with a reference to the global state of the
 * walker, as well as the node triggering the event.
 *
 * TODO: Add ENTRY and EXIT listener events? Perhaps just remove INORDER.
 *
 * TODO: The Walker currently uses the function call stack for traversal --
 * this may change in the future.
 */

#ifndef BRU_WALKER_H
#define BRU_WALKER_H

#include "../sre.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_walker BruWalker;
typedef void              bru_listener_f(void *state, BruRegexNode *curr);

// double pointer to facilitate replacing current node in tree
typedef void bru_walker_f(BruWalker *w, BruRegexNode **curr);

typedef enum {
    BRU_PREORDER,
    BRU_INORDER,
    BRU_POSTORDER,
    BRU_NEVENTS
} BruListenerEvent;

struct bru_walker {
    BruRegexNode **regex;

    // visitor API
    bru_walker_f *walk[BRU_NREGEXTYPES];
    bru_walker_f *walk_terminal;

    // listener API
    bru_listener_f *trigger[BRU_NEVENTS][BRU_NREGEXTYPES];
    bru_listener_f *listen_terminal;

    // global state accessible to walker and listener functions
    void *state;
};

#if !defined(BRU_RE_WALKER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_RE_WALKER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_RE_DISABLE_SHORT_NAMES) &&       \
         (defined(BRU_RE_ENABLE_SHORT_NAMES) ||    \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef bru_walker_f     walker_f;
typedef bru_listener_f   listener_f;
typedef BruListenerEvent ListenerEvent;
#    define PREORDER  BRU_PREORDER
#    define INORDER   BRU_INORDER
#    define POSTORDER BRU_POSTORDER
#    define NEVENTS   BRU_NEVENTS

typedef BruWalker Walker;

#    define WALKER BRU_WALKER
#    define REGEX  BRU_REGEX

#    define WALK         BRU_WALK
#    define WALKER_F     BRU_WALKER_F
#    define SET_WALKER_F BRU_SET_WALKER_F

#    define WALK_TERMINAL       BRU_WALK_TERMINAL
#    define WALK_TERMINAL_F     BRU_WALK_TERMINAL_F
#    define SET_WALK_TERMINAL_F BRU_SET_WALK_TERMINAL_F

#    define CURRENT BRU_CURRENT
#    define LEFT    BRU_LEFT
#    define RIGHT   BRU_RIGHT
#    define CHILD   BRU_CHILD

#    define WALK_LEFT  BRU_WALK_LEFT
#    define WALK_RIGHT BRU_WALK_RIGHT

#    define TRIGGER          BRU_TRIGGER
#    define TRIGGER_TERMINAL BRU_TRIGGER_TERMINAL

#    define SET_CURRENT BRU_SET_CURRENT
#    define SET_LEFT    BRU_SET_LEFT
#    define SET_RIGHT   BRU_SET_RIGHT
#    define SET_CHILD   BRU_SET_CHILD

#    define STATE     BRU_STATE
#    define GET_STATE BRU_GET_STATE

#    define LISTENER_F                 BRU_LISTENER_F
#    define REGISTER_LISTENER_F        BRU_REGISTER_LISTENER_F
#    define LISTEN_TERMINAL_F          BRU_LISTEN_TERMINAL_F
#    define REGISTER_LISTEN_TERMINAL_F BRU_REGISTER_LISTEN_TERMINAL_F

#    define walker_new  bru_walker_new
#    define walker_walk bru_walker_walk
#    define walker_free bru_walker_free
#endif /* BRU_WALKER_ENABLE_SHORT_NAMES */

/* --- Convenience macros --------------------------------------------------- */

#define BRU_WALKER _w
#define BRU_REGEX  _r

#define BRU_WALK(w, r) (w)->walk[(r)->type]((w), &(r))
#define BRU_WALKER_F(type) \
    static void __walk_##type(BruWalker *BRU_WALKER, BruRegexNode **BRU_REGEX)
#define BRU_SET_WALKER_F(w, type) (w)->walk[type] = __walk_##type

#define BRU_WALK_TERMINAL() BRU_WALKER->walk_terminal(BRU_WALKER, BRU_REGEX)
#define BRU_WALK_TERMINAL_F() \
    static void __walk_terminal(BruWalker *BRU_WALKER, BruRegexNode **BRU_REGEX)
#define BRU_SET_WALK_TERMINAL_F(w) (w)->walk_terminal = __walk_terminal

#define BRU_CURRENT (*BRU_REGEX)
#define BRU_LEFT    BRU_CURRENT->left
#define BRU_RIGHT   BRU_CURRENT->right
#define BRU_CHILD   BRU_LEFT

#define BRU_WALK_LEFT() \
    if (BRU_LEFT) BRU_WALK(BRU_WALKER, BRU_LEFT)
#define BRU_WALK_RIGHT() \
    if (BRU_RIGHT) BRU_WALK(BRU_WALKER, BRU_RIGHT)

#define BRU_TRIGGER(event)                                           \
    if (BRU_WALKER->trigger[event][BRU_CURRENT->type])               \
    BRU_WALKER->trigger[event][BRU_CURRENT->type](BRU_WALKER->state, \
                                                  BRU_CURRENT)

#define BRU_TRIGGER_TERMINAL()       \
    if (BRU_WALKER->listen_terminal) \
    BRU_WALKER->listen_terminal(BRU_WALKER->state, BRU_CURRENT)

#define BRU_SET_CURRENT(exp) *BRU_REGEX = exp
#define BRU_SET_LEFT(exp)    BRU_LEFT = exp
#define BRU_SET_RIGHT(exp)   BRU_RIGHT = exp
#define BRU_SET_CHILD(exp)   BRU_CHILD = exp

#define BRU_STATE _state
#define BRU_LISTENER_F(event, type)                                \
    static void __listen_##event##_##type(void         *BRU_STATE, \
                                          BruRegexNode *BRU_REGEX)

#define BRU_GET_STATE(cast, varname) cast varname = (cast) (BRU_STATE)

#define BRU_REGISTER_LISTENER_F(w, event, type) \
    (w)->trigger[event][type] = __listen_##event##_##type

#define BRU_LISTEN_TERMINAL_F() \
    static void __listen_terminal(void *BRU_STATE, BruRegexNode *BRU_REGEX)

#define BRU_REGISTER_LISTEN_TERMINAL_F(w) \
    (w)->listen_terminal = __listen_terminal

/* --- API function prototypes ---------------------------------------------- */

/**
 * Create a generic walker.
 *
 * @return the constructed generic walker
 */
BruWalker *bru_walker_new(void);

/**
 * Walk the given walker from the root node.
 *
 * The return value is for convenience in using the walker consecutively,
 * although in general one should pass a pointer to the variable that is the
 * tree. The walker should then update the variable in place, meaning the
 * return value here is likely not necessary.
 *
 * @param[in] walker the walker
 * @param[in] regex  a pointer to the parsed regular expression to walk
 *
 * @return NULL if any arguments are NULL (including *r), else the previously
 *         walked regex (NULL on the first call)
 */
BruRegexNode *bru_walker_walk(BruWalker *walker, BruRegexNode **regex);

/**
 * Free the memory used by the given walker, returning the underlying regex.
 *
 * @param[in] walker the walker
 *
 * @return the underlying regex
 */
BruRegexNode *bru_walker_free(BruWalker *walker);

#endif /* BRU_WALKER_H */
