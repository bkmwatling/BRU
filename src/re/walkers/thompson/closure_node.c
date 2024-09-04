#include "closure_node.h"

/* --- Walk functions ------------------------------------------------------- */

BRU_WALKER_F(BRU_STAR)
{
    BRU_WALK_LEFT();
    // insert memoisation instruction behind child of star
    // F* -> (#F)*
    BRU_SET_CHILD(
        bru_regex_branch(BRU_CONCAT, bru_regex_new(BRU_MEMOISE), BRU_CHILD));
}

BRU_WALKER_F(BRU_PLUS)
{
    BRU_WALK_LEFT();
    // insert memoisation instruction behind child of plus
    // F+ -> (#F)+
    BRU_SET_CHILD(
        bru_regex_branch(BRU_CONCAT, bru_regex_new(BRU_MEMOISE), BRU_CHILD));
}

/* --- API function definitions --------------------------------------------- */

void bru_closure_node_thompson(BruRegexNode **r)
{
    BruWalker *w;

    if (!r || !(*r)) return;

    w = bru_walker_new();
    BRU_SET_WALKER_F(w, BRU_STAR);
    BRU_SET_WALKER_F(w, BRU_PLUS);

    bru_walker_walk(w, r);
    bru_walker_free(w);
}
