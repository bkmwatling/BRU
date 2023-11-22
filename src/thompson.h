#ifndef THOMPSON_H
#define THOMPSON_H

#include "compiler.h"
#include "sre.h"

const Program *thompson_compile(const Regex *re, const CompilerOpts *opts);

#endif /* THOMPSON_H */
