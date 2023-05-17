#ifndef PARSER_H
#define PARSER_H

#include "sre.h"
#include "types.h"

typedef struct {
    int in_group;
    int in_lookahead;
} ParseState;

typedef struct {
    uint *regex;
    int   only_counters;
    int   unbounded_counters;
    int   whole_match_capture;
} Parser;

uint *str_to_regex(char *s);

ParseState *parse_state(int in_group, int in_lookahead);

Parser    *parser(uint *regex,
                  int   only_counters,
                  int   unbounded_counters,
                  int   whole_match_capture);
RegexTree *parse(Parser *p);

#endif /* PARSER_H */
