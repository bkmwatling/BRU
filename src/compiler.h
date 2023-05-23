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
    Parser      *parser;
    Construction construction;
    int          only_std_split;
    SplitChoice  branch;
} Compiler;

Program *compile(Compiler *compiler);

void compiler_free(Compiler *compiler);

#endif /* COMPILER_H */
