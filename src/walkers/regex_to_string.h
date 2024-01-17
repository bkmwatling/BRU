#ifndef _REGEX_TO_STR_H
#define _REGEX_TO_STR_H

#include "walker.h"

/**
 * Convert the given regex tree to a string.
 *
 * @param re The regular expression
 */
char *regex_to_string(Regex *re);

#endif
