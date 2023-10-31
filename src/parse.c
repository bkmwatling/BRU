#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "stc/util/args.h"
#include "utf8.h"

int main(int argc, const char **argv)
{
    char      *regex, *re_tree;
    Parser    *p;
    Regex     *re;
    ParserOpts opts;
    StcArg     args[] = {
        { STC_ARG_BOOL, "-o", "--only-counters", &opts.only_counters, NULL,
              "whether to use just counters and treat *, +, and ? as counters",
              NULL, NULL },
        { STC_ARG_BOOL, "-u", "--unbounded-counters", &opts.unbounded_counters,
              NULL, "whether to permit unbounded counters or substitute with *",
              NULL, NULL },
        { STC_ARG_BOOL, "-w", "--whole-match-capture",
              &opts.whole_match_capture, NULL,
              "whether to have the whole regex match be the 0th capture", NULL,
              NULL },
        {
            STC_ARG_STR,
            "<regex>",
            NULL,
            &re_tree,
            NULL,
            "the regex to parse",
            NULL,
            NULL,
        }
    };

    stc_args_parse_exact(argc, argv, args, 4, NULL);

    regex = malloc((strlen(re_tree) + 1) * sizeof(char));
    strcpy(regex, re_tree);
    p       = parser_new(regex, &opts);
    re      = parse(p);
    re_tree = regex_to_tree_str(re);

    printf("%s\n", re_tree);

    parser_free(p);
    regex_free(re);
    free(re_tree);

    return EXIT_SUCCESS;
}
