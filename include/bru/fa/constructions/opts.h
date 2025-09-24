#ifndef BRU_FA_CONSTRUCTION_OPTS_H
#define BRU_FA_CONSTRUCTION_OPTS_H

typedef enum { BRU_CS_PCRE, BRU_CS_RE2 } BruCaptureSemantics;

typedef struct {
    BruCaptureSemantics capture_semantics;
} BruConstructionOpts;

#if !defined(BRU_FA_CONSTRUCTION_OPTS_DISABLE_SHORT_NAMES) && \
    (defined(BRU_FA_CONSTRUCTION_OPTS_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_FA_DISABLE_SHORT_NAMES) &&                  \
         (defined(BRU_FA_ENABLE_SHORT_NAMES) ||               \
          defined(BRU_ENABLE_SHORT_NAMES)))

#    define CS_PCRE BRU_CS_PCRE
#    define CS_RE2  BRU_CS_RE2

typedef BruCaptureSemantics CaptureSemantics;
typedef BruConstructionOpts ConstructionOpts;
#endif /* BRU_FA_CONSTRUCTION_OPTS_ENABLE_SHORT_NAMES */

#endif /* BRU_FA_CONSTRUCTION_OPTS_H */
