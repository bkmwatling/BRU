#ifndef COMPILER_H
#define COMPILER_H

#include "parser.h"
#include "srvm.h"

typedef enum {
    THOMPSON,
    GLUSHKOV,
} Construction;

typedef enum {
    SC_SPLIT,
    SC_TSWITCH,
    SC_LSWITCH,
} SplitChoice;

typedef struct {
    const Parser *parser;
    Construction  construction;
    int           only_std_split;
    SplitChoice   branch;
} Compiler;

Compiler *compiler(const Parser *p,
                   Construction  construction,
                   int           only_std_split,
                   SplitChoice   branch);
void      compiler_free(Compiler *compiler);
Program  *compile(const Compiler *compiler);

#endif /* COMPILER_H */
