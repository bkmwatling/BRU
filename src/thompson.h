#ifndef THOMPSON_H
#define THOMPSON_H

#include "compiler.h"
#include "sre.h"

const Program *thompson_compile(const Regex *re);

#endif /* THOMPSON_H */
