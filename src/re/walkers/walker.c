#include <stdlib.h>
#include <string.h>

#include "walker.h"

/* --- Identity walker ------------------------------------------------------ */

BRU_WALK_TERMINAL_F() { BRU_TRIGGER_TERMINAL(); }

// one for each RegexType defined in sre.h
BRU_WALKER_F(BRU_CARET) { BRU_WALK_TERMINAL(); }

BRU_WALKER_F(BRU_DOLLAR) { BRU_WALK_TERMINAL(); }

BRU_WALKER_F(BRU_MEMOISE) { BRU_WALK_TERMINAL(); }

BRU_WALKER_F(BRU_LITERAL) { BRU_WALK_TERMINAL(); }

BRU_WALKER_F(BRU_CC) { BRU_WALK_TERMINAL(); }

BRU_WALKER_F(BRU_ALT)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_WALK_RIGHT();
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_WALKER_F(BRU_CONCAT)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_WALK_RIGHT();
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_WALKER_F(BRU_CAPTURE)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_WALKER_F(BRU_STAR)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_WALKER_F(BRU_PLUS)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_WALKER_F(BRU_QUES)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_WALKER_F(BRU_COUNTER)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_WALK_LEFT();
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);
}

BRU_WALKER_F(BRU_LOOKAHEAD)
{
    BRU_TRIGGER(BRU_PREORDER);
    BRU_TRIGGER(BRU_INORDER);
    BRU_TRIGGER(BRU_POSTORDER);
}

/* --- API function definitions --------------------------------------------- */

BruWalker *bru_walker_new(void)
{
    BruWalker *w;

    w = malloc(sizeof(*w));
    if (!w) return NULL;

    w->regex           = NULL;
    w->state           = NULL;
    w->listen_terminal = NULL;
    memset(w->walk, 0, sizeof(w->walk));
    memset(w->trigger, 0, sizeof(w->trigger));

    BRU_SET_WALK_TERMINAL_F(w);
    BRU_SET_WALKER_F(w, BRU_CARET);
    BRU_SET_WALKER_F(w, BRU_DOLLAR);
    /* BRU_SET_WALKER_F(w, BRU_MEMOISE); */
    BRU_SET_WALKER_F(w, BRU_LITERAL);
    BRU_SET_WALKER_F(w, BRU_CC);
    BRU_SET_WALKER_F(w, BRU_ALT);
    BRU_SET_WALKER_F(w, BRU_CONCAT);
    BRU_SET_WALKER_F(w, BRU_CAPTURE);
    BRU_SET_WALKER_F(w, BRU_STAR);
    BRU_SET_WALKER_F(w, BRU_PLUS);
    BRU_SET_WALKER_F(w, BRU_QUES);
    BRU_SET_WALKER_F(w, BRU_COUNTER);
    BRU_SET_WALKER_F(w, BRU_LOOKAHEAD);

    return w;
}

BruRegexNode *bru_walker_walk(BruWalker *walker, BruRegexNode **regex)
{
    BruRegexNode *prev = NULL;

    if (!walker || !regex || !(*regex)) return NULL;

    if (walker->regex) prev = *walker->regex;

    walker->regex = regex;
    BRU_WALK(walker, *(walker->regex));

    return prev;
}

BruRegexNode *bru_walker_free(BruWalker *walker)
{
    BruRegexNode *out = NULL;

    if (walker) {
        out = *walker->regex;
        free(walker);
    }

    return out;
}
