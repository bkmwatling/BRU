#include "in_degree.h"

#define NULLABLE 0x1
#define LAST1    0x2

#define HAS &
#define AND &
#define OR  |
#define NOT ~

#define SET_NULLABLE(x, f) SET_FLAG(x, f)
#define SET_LAST1(x, f)    SET_FLAG(x, f)
#define SET_FLAG(x, f)     x |= (f)
#define CLEAR_FLAG(x, f)   x &= ~(f)

/* --- Walk functions (Glushkov) -------------------------------------------- */

BRU_WALKER_F(BRU_ALT)
{
    bru_byte_t *state = BRU_WALKER->state;
    bru_byte_t  left;

    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();

    // save info
    left = *state;

    BRU_TRIGGER(BRU_INORDER);
    BRU_WALK_RIGHT();
    BRU_TRIGGER(BRU_POSTORDER);

    SET_NULLABLE(*state, (left OR * state) HAS NULLABLE);
    SET_FLAG(*state, LAST1);
}

BRU_WALKER_F(BRU_CONCAT)
{
    bru_byte_t *state = BRU_WALKER->state;
    bru_byte_t  left;

    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();

    // save left state
    left = *state;

    if (left HAS LAST1)
        BRU_SET_LEFT(
            bru_regex_branch(BRU_CONCAT, BRU_LEFT, bru_regex_new(BRU_MEMOISE)));

    BRU_TRIGGER(BRU_INORDER);
    BRU_WALK_RIGHT();
    BRU_TRIGGER(BRU_POSTORDER);

    SET_NULLABLE(*state, (left AND * state)
                             HAS NULLABLE AND NOT((left HAS LAST1) >> 1));
    SET_LAST1(*state, (*state HAS NULLABLE) << 1);
}

BRU_WALKER_F(BRU_CAPTURE)
{
    bru_byte_t *state = BRU_WALKER->state;

    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);

    if (*state HAS LAST1)
        BRU_SET_CHILD(bru_regex_branch(BRU_CONCAT, BRU_CHILD,
                                       bru_regex_new(BRU_MEMOISE)));

    CLEAR_FLAG(*state, NULLABLE OR LAST1);
}

BRU_WALKER_F(BRU_STAR)
{
    bru_byte_t *state = BRU_WALKER->state;

    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);

    BRU_SET_CHILD(
        bru_regex_branch(BRU_CONCAT, bru_regex_new(BRU_MEMOISE), BRU_CHILD));
    SET_FLAG(*state, NULLABLE);
}

BRU_WALKER_F(BRU_PLUS)
{
    bru_byte_t *state = BRU_WALKER->state;

    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);

    BRU_SET_CHILD(
        bru_regex_branch(BRU_CONCAT, bru_regex_new(BRU_MEMOISE), BRU_CHILD));
    // child no longer nullable thanks to memoisation
    CLEAR_FLAG(*state, NULLABLE);
}

BRU_WALKER_F(BRU_QUES)
{
    bru_byte_t *state = BRU_WALKER->state;

    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);

    SET_FLAG(*state, NULLABLE);
}

BRU_WALKER_F(BRU_COUNTER)
{
    bru_byte_t *state = BRU_WALKER->state;

    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);

    // if we can match more than once, then we can enter
    // the subexpression through multiple paths -> prepend
    // memoisation instruction
    // (ab{0,5} => a(?:#b){0,5})

    if (BRU_CURRENT->max > 1) {
        // can match multiple times; requires memoisation
        BRU_SET_CHILD(bru_regex_branch(BRU_CONCAT, bru_regex_new(BRU_MEMOISE),
                                       BRU_CHILD));

        // child is no longer nullable
        CLEAR_FLAG(*state, NULLABLE);
    }

    if (BRU_CURRENT->min == 0) {
        // nullable
        SET_FLAG(*state, NULLABLE);
    }
}

BRU_WALKER_F(BRU_LOOKAHEAD)
{
    // TODO: no idea how lookahead is implemented
    BRU_TRIGGER(BRU_PREORDER);
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_LISTEN_TERMINAL_F()
{
    BRU_GET_STATE(bru_byte_t *, state);
    CLEAR_FLAG(*state, NULLABLE OR LAST1);

    (void) BRU_REGEX;
}

void bru_in_degree_glushkov(BruRegexNode **r)
{
    BruWalker *w;
    bru_byte_t last;

    if (!r || !(*r)) return;

    w        = bru_walker_new();
    w->state = &last;

    BRU_SET_WALKER_F(w, BRU_ALT);
    BRU_SET_WALKER_F(w, BRU_CONCAT);
    BRU_SET_WALKER_F(w, BRU_CAPTURE);
    BRU_SET_WALKER_F(w, BRU_STAR);
    BRU_SET_WALKER_F(w, BRU_PLUS);
    BRU_SET_WALKER_F(w, BRU_QUES);
    BRU_SET_WALKER_F(w, BRU_COUNTER);
    BRU_SET_WALKER_F(w, BRU_LOOKAHEAD);

    BRU_REGISTER_LISTEN_TERMINAL_F(w);

    bru_walker_walk(w, r);
    bru_walker_free(w);
}
