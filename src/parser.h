#ifndef PARSER_H
#define PARSER_H

#include "sre.h"

typedef struct {
    int only_counters;
    int unbounded_counters;
    int whole_match_capture;
} ParserOpts;

typedef struct {
    const char *regex;
    ParserOpts  opts;
} Parser;

Parser *parser_new(const char *regex, ParserOpts *opts);
void    parser_free(Parser *p);
Regex  *parse(const Parser *p);

#endif /* PARSER_H */
