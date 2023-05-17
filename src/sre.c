#include <stdio.h>
#include <stdlib.h>

#include "sre.h"

RegexTree *regex_tree_single_child(RegexKind kind, RegexTree *child, int pos)
{
    RegexTree *re_tree = malloc(sizeof(RegexTree));

    /* TODO: check `kind` to make sure correct node type */

    re_tree->kind  = kind;
    re_tree->child = child;
    re_tree->pos   = pos;

    return re_tree;
}
