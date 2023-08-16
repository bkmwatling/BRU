#ifndef TYPES_H
#define TYPES_H

#define TRUE     1
#define FALSE    0
#define UINT_MAX ((uint) ~0)
#define CNTR_MAX ((cntr_t) ~0)

#define LEN_FMT    "%hu"
#define CNTR_FMT   "%u"
#define OFFSET_FMT "%d"

typedef char           byte;
typedef unsigned int   uint;
typedef unsigned short len_t;
typedef unsigned short cntr_t;
typedef unsigned short mem_t;
typedef int            offset_t;

#endif /* TYPES_H */
