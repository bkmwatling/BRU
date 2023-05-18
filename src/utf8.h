#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>

#define UTF8_IS_CONTINUATION(c) (((c) &0xc0) == 0x80)
#define UTF8_IS_SINGLE(s)       (((s)[0] & 0x80) == 0x0)
#define UTF8_IS_DOUBLE(s) \
    (((s)[0] & 0xe0) == 0xc0 && UTF8_IS_CONTINUATION((s)[1]))
#define UTF8_IS_TRIPLE(s)                                       \
    (((s)[0] & 0xf0) == 0xe0 && UTF8_IS_CONTINUATION((s)[1]) && \
     UTF8_IS_CONTINUATION((s)[2]))
#define UTF8_IS_QUADRUPLE(s)                                    \
    (((s)[0] & 0xf8) == 0xf0 && UTF8_IS_CONTINUATION((s)[1]) && \
     UTF8_IS_CONTINUATION((s)[2]) && UTF8_IS_CONTINUATION((s)[3]))

typedef unsigned int utf8;

int    utf8_is_valid(const char *s);
size_t utf8_num_bytes(const char *s);
size_t utf8_len_from(const char *s);
utf8  *utf8_from_str(const char *s);

size_t utf8_charlen(const utf8 utf8_c);
size_t utf8_strlen(const utf8 *utf8_s);
size_t utf8len(const utf8 *utf8_s);
char  *utf8_to_str(const utf8 *utf8_s);

#endif /* UTF8_H */
