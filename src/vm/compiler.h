#ifndef COMPILER_H
#define COMPILER_H

#include "../re/parser.h"
#include "program.h"

typedef enum {
    THOMPSON,
    GLUSHKOV,
} Construction;

typedef enum {
    SC_SPLIT,
    SC_TSWITCH,
} SplitChoice;

typedef enum {
    CS_PCRE,
    CS_RE2,
} CaptureSemantics;

typedef enum {
    MS_NONE,
    MS_CN,
    MS_IN,
    MS_IAR,
} MemoScheme;

typedef struct {
    Construction     construction;
    int              only_std_split;
    SplitChoice      branch;
    CaptureSemantics capture_semantics;
    MemoScheme       memo_scheme;
} CompilerOpts;

typedef struct {
    const Parser *parser;
    CompilerOpts  opts;
} Compiler;

Compiler      *compiler_new(const Parser *parser, const CompilerOpts *opts);
void           compiler_free(Compiler *self);
const Program *compiler_compile(const Compiler *self);

#endif /* COMPILER_H */
