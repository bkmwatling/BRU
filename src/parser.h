#ifndef PARSER_H
#define PARSER_H

#include "sre.h"
#include "types.h"
#include "utf8.h"

typedef struct {
    int in_group;
    int in_lookahead;
} ParseState;

typedef struct {
    const char *regex;
    int         only_counters;
    int         unbounded_counters;
    int         whole_match_capture;
} Parser;

ParseState *parse_state(int in_group, int in_lookahead);

Parser *parser(const char *regex,
               int         only_counters,
               int         unbounded_counters,
               int         whole_match_capture);
void    parser_free(Parser *p);
Regex  *parse(const Parser *p);

#endif /* PARSER_H */
