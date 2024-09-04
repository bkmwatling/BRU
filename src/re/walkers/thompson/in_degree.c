#include "in_degree.h"

/* --- Walk functions (Thompson) -------------------------------------------- */

BRU_WALKER_F(BRU_STAR)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);

    // insert memoisation instruction behind child of star and after star
    // F* -> (#F)*#
    BRU_SET_CHILD(
        bru_regex_branch(BRU_CONCAT, bru_regex_new(BRU_MEMOISE), BRU_CHILD));
    BRU_SET_CURRENT(
        bru_regex_branch(BRU_CONCAT, BRU_CURRENT, bru_regex_new(BRU_MEMOISE)));
}

BRU_WALKER_F(BRU_ALT)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_WALK_RIGHT();
    BRU_TRIGGER(BRU_POSTORDER);

    // insert memoisation instruction after alternation
    // (A|B) -> (A|B)#
    BRU_SET_CURRENT(
        bru_regex_branch(BRU_CONCAT, BRU_CURRENT, bru_regex_new(BRU_MEMOISE)));
}

BRU_WALKER_F(BRU_QUES)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);

    // insert memoisation instruction after optional expression
    // E? -> E?#
    BRU_SET_CURRENT(
        bru_regex_branch(BRU_CONCAT, BRU_CURRENT, bru_regex_new(BRU_MEMOISE)));
}

/* --- API function definitions --------------------------------------------- */

void bru_in_degree_thompson(BruRegexNode **r)
{
    BruWalker *w;

    if (!r || !(*r)) return;

    w = bru_walker_new();
    BRU_SET_WALKER_F(w, BRU_STAR);
    BRU_SET_WALKER_F(w, BRU_ALT);
    BRU_SET_WALKER_F(w, BRU_QUES);

    bru_walker_walk(w, r);
    bru_walker_free(w);
}
