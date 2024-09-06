#ifndef BRU_UTILS_H
#define BRU_UTILS_H

#if !defined(BRU_UTILS_DISABLE_SHORT_NAMES) && \
    (defined(BRU_UTILS_ENABLE_SHORT_NAMES) || defined(BRU_ENABLE_SHORT_NAMES))
#    define UNUSED       BRU_UNUSED
#    define ENSURE_SPACE BRU_ENSURE_SPACE
#    define PUSH         BRU_PUSH
#    define STR_PUSH     BRU_STR_PUSH

#    define DLL_INIT       BRU_DLL_INIT
#    define DLL_FREE       BRU_DLL_FREE
#    define DLL_PUSH_FRONT BRU_DLL_PUSH_FRONT
#    define DLL_PUSH_BACK  BRU_DLL_PUSH_BACK
#    define DLL_GET        BRU_DLL_GET
#endif /* BRU_UTILS_ENABLE_SHORT_NAMES */

#define BRU_UNUSED(var) ((void) &var)

#define BRU_ENSURE_SPACE(arr, len, alloc, size)       \
    do {                                              \
        if ((len) >= (alloc)) {                       \
            do {                                      \
                (alloc) <<= 1;                        \
            } while ((len) >= (alloc));               \
            (arr) = realloc((arr), (alloc) * (size)); \
        }                                             \
    } while (0)

#define BRU_PUSH(arr, len, alloc, item)                        \
    do {                                                       \
        BRU_ENSURE_SPACE(arr, (len) + 1, alloc, sizeof(item)); \
        (arr)[(len)++] = (item);                               \
    } while (0)

#define BRU_STR_PUSH(s, len, alloc, str)                               \
    do {                                                               \
        BRU_ENSURE_SPACE(s, (len) + sizeof(str), alloc, sizeof(char)); \
        strncpy((s) + (len), (str), (alloc) - (len));                  \
        (len) += sizeof(str) - 1;                                      \
    } while (0)

#define BRU_DLL_INIT(dll)                        \
    do {                                         \
        (dll)       = calloc(1, sizeof(*(dll))); \
        (dll)->prev = (dll)->next = (dll);       \
    } while (0)

#define BRU_DLL_FREE(dll, elem_free, elem, next)                       \
    do {                                                               \
        for ((elem) = (dll)->next; (elem) != (dll); (elem) = (next)) { \
            (next) = (elem)->next;                                     \
            (elem_free)((elem));                                       \
        }                                                              \
        free((dll));                                                   \
    } while (0)

#define BRU_DLL_PUSH_FRONT(dll, elem)     \
    do {                                  \
        (elem)->prev       = (dll);       \
        (elem)->next       = (dll)->next; \
        (elem)->next->prev = (elem);      \
        (dll)->next        = (elem);      \
    } while (0)

#define BRU_DLL_PUSH_BACK(dll, elem)      \
    do {                                  \
        (elem)->prev       = (dll)->prev; \
        (elem)->next       = (dll);       \
        (dll)->prev        = (elem);      \
        (elem)->prev->next = (elem);      \
    } while (0)

#define BRU_DLL_GET(dll, idx, elem)                          \
    for ((elem) = (dll)->next; (idx) > 0 && (elem) != (dll); \
         (idx)--, (elem) = (elem)->next)

#endif /* BRU_UTILS_H */
