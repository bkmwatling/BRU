#ifndef BRU_FA_TRANSFORM_OPTS_H
#define BRU_FA_TRANSFORM_OPTS_H

typedef enum { BRU_MS_NONE, BRU_MS_IN, BRU_MS_CN, BRU_MS_IAR } BruMemoScheme;

#if !defined(BRU_FA_TRANSFORM_OPTS_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_TRANSFORM_OPTS_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&               \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||            \
          defined(BRU_ENABLE_SHORT_NAMES)))

typedef BruMemoScheme MemoScheme;
#endif /* BRU_FA_TRANSFORM_OPTS_ENABLE_SHORT_NAMES */

#endif
