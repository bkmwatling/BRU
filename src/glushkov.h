#ifndef GLUSHKOV_H
#define GLUSHKOV_H

#include "compiler.h"
#include "smir.h"

StateMachine *glushkov_construct(Regex re, const CompilerOpts *opts);

#endif /* GLUSHKOV_H */
