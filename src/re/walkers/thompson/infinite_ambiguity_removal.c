#include "infinite_ambiguity_removal.h"

/* --- API function definitions --------------------------------------------- */

void bru_infinite_ambiguity_removal_thompson(BruRegexNode **r)
{
    BruWalker *w;

    if (!r || !(*r)) return;

    w = bru_walker_new();
    // TODO: add walker functions

    bru_walker_walk(w, r);
    bru_walker_free(w);
}
