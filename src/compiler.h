#ifndef COMPILER_H
#define COMPILER_H

#include "parser.h"
#include "program.h"

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
    Construction construction;
    int          only_std_split;
    SplitChoice  branch;
} CompilerOpts;

typedef struct {
    const Parser *parser;
    CompilerOpts  opts;
} Compiler;

Compiler      *compiler_new(const Parser *parser, const CompilerOpts *opts);
void           compiler_free(Compiler *self);
const Program *compiler_compile(const Compiler *self);

#endif /* COMPILER_H */
