#ifndef GLUSHKOV_H
#define GLUSHKOV_H

#include "../../vm/compiler.h"
#include "../smir.h"

StateMachine *glushkov_construct(Regex re, const CompilerOpts *opts);

#endif /* GLUSHKOV_H */
