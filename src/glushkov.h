#ifndef GLUSHKOV_H
#define GLUSHKOV_H

#include "compiler.h"
#include "sre.h"

const Program *glushkov_compile(const Regex *re, const CompilerOpts *opts);

#endif /* GLUSHKOV_H */
