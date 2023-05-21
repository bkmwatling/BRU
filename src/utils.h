#ifndef UTILS_H
#define UTILS_H

#define ENSURE_SPACE(arr, len, alloc, size)       \
    if ((len) >= (alloc)) {                       \
        do {                                      \
            (alloc) *= 2;                         \
        } while ((len) >= (alloc));               \
        (arr) = realloc((arr), (alloc) * (size)); \
    }

#define PUSH(arr, len, alloc, item)                    \
    ENSURE_SPACE(arr, (len) + 1, alloc, sizeof(item)); \
    (arr)[(len)++] = (item)

#endif /* UTILS_H */
