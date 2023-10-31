#ifndef UTILS_H
#define UTILS_H

#define ENSURE_SPACE(arr, len, alloc, size)       \
    if ((len) >= (alloc)) {                       \
        do {                                      \
            (alloc) <<= 1;                        \
        } while ((len) >= (alloc));               \
        (arr) = realloc((arr), (alloc) * (size)); \
    }

#define PUSH(arr, len, alloc, item)                    \
    ENSURE_SPACE(arr, (len) + 1, alloc, sizeof(item)); \
    (arr)[(len)++] = (item)

#define STR_PUSH(s, len, alloc, str)                           \
    ENSURE_SPACE(s, (len) + sizeof(str), alloc, sizeof(char)); \
    strncpy((s) + (len), (str), (alloc) - (len));              \
    (len) += sizeof(str) - 1

#endif /* UTILS_H */
