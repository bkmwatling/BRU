#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "utf8.h"

int main(int argc, char **argv)
{
    char   *regex, *re_tree;
    Parser *p;
    Regex  *re;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <regex>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    regex = malloc((strlen(argv[1]) + 1) * sizeof(char));
    strcpy(regex, argv[1]);
    p       = parser(regex, FALSE, FALSE, FALSE);
    re      = parse(p);
    re_tree = regex_to_tree_str(re);

    printf("%s\n", re_tree);

    parser_free(p);
    regex_free(re);
    free(re_tree);

    return EXIT_SUCCESS;
}
