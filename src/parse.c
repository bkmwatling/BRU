#include <stdio.h>
#include <stdlib.h>

#include "parser.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <regex>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    uint   *regex = str_to_regex(argv[1]);
    Parser *p     = parser(regex, 0, 0, 0);

    printf("regex:");
    while (*regex) { printf(" %u", *regex++); }
    printf("\nparsed regex: %p\n", (void *) parse(p));

    return EXIT_SUCCESS;
}
