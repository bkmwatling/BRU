#ifndef THOMPSON_H
#define THOMPSON_H

#include "../../vm/compiler.h"
#include "../smir.h"

StateMachine *thompson_construct(Regex re, const CompilerOpts *opts);

#endif /* THOMPSON_H */
