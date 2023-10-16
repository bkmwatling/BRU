#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "parser.h"
#include "srvm.h"

int main(int argc, char **argv)
{
    char     *regex, *prog_str;
    Compiler *c;
    Program  *prog;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <regex>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    regex = malloc((strlen(argv[1]) + 1) * sizeof(char));
    strcpy(regex, argv[1]);
    c        = compiler_new(parser_new(regex, NULL), NULL);
    prog     = compile(c);
    prog_str = program_to_str(prog);

    printf("%s\n", prog_str);

    compiler_free(c);
    program_free(prog);
    free(prog_str);

    return EXIT_SUCCESS;
}
