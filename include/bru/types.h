#ifndef BRU_TYPES_H
#define BRU_TYPES_H

#define TRUE         1
#define FALSE        0
#define BRU_UINT_MAX ((bru_uint_t) ~0)
#define BRU_CNTR_MAX ((bru_cntr_t) ~0)

#define BRU_LEN_FMT    "%hu"
#define BRU_CNTR_FMT   "%hu"
#define BRU_MEM_FMT    "%hu"
#define BRU_OFFSET_FMT "%d"

typedef char           bru_byte_t;
typedef unsigned int   bru_uint_t;
typedef unsigned short bru_len_t;
typedef unsigned short bru_cntr_t;
typedef unsigned short bru_mem_t;
typedef int            bru_offset_t;

#if !defined(BRU_TYPES_DISABLE_SHORT_NAMES) && \
    (defined(BRU_TYPES_ENABLE_SHORT_NAMES) || defined(BRU_ENABLE_SHORT_NAMES))
#    define UINT_MAX BRU_UINT_MAX
#    define CNTR_MAX BRU_CNTR_MAX

#    define LEN_FMT    BRU_LEN_FMT
#    define CNTR_FMT   BRU_CNTR_FMT
#    define MEM_FMT    BRU_MEM_FMT
#    define OFFSET_FMT BRU_OFFSET_FMT

typedef bru_byte_t   byte_t;
typedef bru_uint_t   uint_t;
typedef bru_len_t    len_t;
typedef bru_cntr_t   cntr_t;
typedef bru_mem_t    mem_t;
typedef bru_offset_t offset_t;
#endif /* BRU_TYPES_ENABLE_SHORT_NAMES */

#endif /* BRU_TYPES_H */
