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

size_t utf8_len_from(const char *s)
{
    size_t len = 0;
    while (*s) {
        if (!UTF8_IS_CONTINUATION(*s++)) ++len;
    }
    return len;
}

utf8 *utf8_from_str(const char *s)
{
    utf8  *utf8_s = malloc((utf8_len_from(s) + 1) * sizeof(utf8));
    utf8  *p      = utf8_s;
    size_t i, len;

    while (*s) {
        if ((len = utf8_num_bytes(s))) {
            *p = 0;
            for (i = 0; i < len; ++i) { *p = (*p << 8) | (*s++ & 0xff); }
            p++;
        } else {
            free(utf8_s);
            return NULL;
        }
    }
    *p = 0;

    return utf8_s;
}

size_t utf8_charlen(const utf8 utf8_c)
{
    size_t len = 0;

    if (utf8_c & 0xff000000) {
        len = 4;
    } else if (utf8_c & 0x00ff0000) {
        len = 3;
    } else if (utf8_c & 0x0000ff00) {
        len = 2;
    } else if (utf8_c & 0x000000ff) {
        len = 1;
    }

    return len;
}

size_t utf8_strlen(const utf8 *utf8_s)
{
    size_t len = 0;
    for (; *utf8_s; utf8_s++) { len += utf8_charlen(*utf8_s); }

    return len;
}

size_t utf8len(const utf8 *utf8_s)
{
    const utf8 *p = utf8_s;
    while (*p++)
        ;

    return p - utf8_s;
}

char *utf8_to_char(const utf8 utf8_c)
{
    char  *s = NULL, *p;
    size_t i, len;

    if ((len = utf8_charlen(utf8_c))) {
        s = malloc((len + 1) * sizeof(char));
        p = s;
        for (i = 0; i < len; ++i) {
            *p++ = (char) (utf8_c >> (8 * (len - 1 - i)));
        }
        *p = '\0';
    }

    return s;
}

char *utf8_to_str(const utf8 *utf8_s)
{
    char  *s = malloc((utf8_strlen(utf8_s) + 1) * sizeof(char)), *p = s;
    size_t i, len;

    while (*utf8_s) {
        if ((len = utf8_charlen(*utf8_s))) {
            for (i = 0; i < len; ++i) {
                *p++ = (char) (*utf8_s >> (8 * (len - 1 - i)));
            }
            ++utf8_s;
        } else {
            free(s);
            return NULL;
        }
    }
    *p = '\0';

    return s;
}
