#include "utils.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char escape(const char s)
{
    const char unescaped[] = { '\a', '\b', '\f', '\n', '\r', '\t', '\v', '\\' };
    const char escaped[]   = { 'a', 'b', 'f', 'n', 'r', 't', 'v', '\\' };
    const size_t len_unescaped = sizeof(unescaped) / sizeof(*unescaped);
    size_t       i;

    for (i = 0; i < len_unescaped; i++)
        if (s == unescaped[i]) return escaped[i];
    return '\0';
}

char *escape_string(const char *s)
{
    char        *t;
    const size_t len_s = strlen(s);
    size_t       i, j;
    size_t       len_t = len_s;

    for (i = 0; i < len_s; i++)
        if (escape(s[i]) != '\0') len_t++;
    t = malloc(len_t + 1);

    for (i = 0, j = 0; i < len_t; i++) {
        const char c = escape(s[i]);
        if (c != '\0') {
            t[j++] = '\\';
            t[j++] = c;
        } else {
            t[j++] = s[i];
        }
    }
    t[j] = '\0';
    return t;
}
