#ifndef PARSER_H
#define PARSER_H

#include "sre.h"

typedef struct {
    int only_counters;
    int unbounded_counters;
    int expand_counters;
    int whole_match_capture;
} ParserOpts;

typedef struct {
    const char *regex;
    ParserOpts  opts;
} Parser;

Parser *parser_new(const char *regex, const ParserOpts *opts);
void    parser_free(Parser *self);
Regex   parser_parse(const Parser *self);

#endif /* PARSER_H */
