#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "utf8.h"

int main(int argc, char **argv)
{
    char *regex;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <regex>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    regex = malloc((strlen(argv[1]) + 1) * sizeof(char));
    strcpy(regex, argv[1]);
    Parser    *p           = parser(regex, 0, 0, 0);
    RegexTree *re_tree     = parse(p);
    char      *re_tree_str = regex_tree_to_tree_str(re_tree);

    printf("%s\n", re_tree_str);

    parser_free(p);
    regex_tree_free(re_tree);
    free(re_tree_str);

    return EXIT_SUCCESS;
}
