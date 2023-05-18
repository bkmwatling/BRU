#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "utf8.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <regex>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    utf8   *regex = utf8_from_str(argv[1]), *r = regex;
    Parser *p = parser(regex, 0, 0, 0);

    printf("utf8 regex:");
    while (*r) { printf(" 0x%x", *r++); }
    printf("\nregex: %s\n", utf8_to_str(regex));
    printf("parsed regex: %p\n", (void *) parse(p));

    return EXIT_SUCCESS;
}
