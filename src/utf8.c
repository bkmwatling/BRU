#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utf8.h"

int utf8_is_valid(const char *s)
{
    size_t num_bytes;

    while (*s) {
        num_bytes = utf8_num_bytes(s);

        if (num_bytes) {
            s += num_bytes;
        } else {
            return 0;
        }
    }

    return 1;
}

size_t utf8_num_bytes(const char *s)
{
    size_t min_len = 0, num_bytes = 0;

    while (min_len < 4 && s[min_len++])
        ;

    if (min_len >= 1 && UTF8_IS_SINGLE(s)) {
        // is valid single byte (ie 0xxx xxxx)
        num_bytes = 1;
    } else if (min_len >= 2 && UTF8_IS_DOUBLE(s)) {
        // or is valid double byte (ie 110x xxxx and continuation byte)
        num_bytes = 2;
    } else if (min_len >= 3 && UTF8_IS_TRIPLE(s)) {
        // or is valid tripple byte (ie 1110 xxxx and continuation byte)
        num_bytes = 3;
    } else if (min_len >= 4 && UTF8_IS_QUADRUPLE(s)) {
        // or is valid tripple byte (ie 1111 0xxx and continuation byte)
        num_bytes = 4;
    }

    return num_bytes;
}

char *utf8_to_str(const char *s)
{
    char  *p;
    size_t n = utf8_num_bytes(s);

    p = malloc((n + 1) * sizeof(char));
    strncpy(p, s, n);
    p[n] = '\0';

    return p;
}

int utf8cmp(const char *a, const char *b, size_t alen, size_t blen)
{
    if (alen != blen) return alen - blen;

    return strncmp(a, b, alen);
}
