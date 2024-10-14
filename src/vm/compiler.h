#ifndef BRU_VM_COMPILER_H
#define BRU_VM_COMPILER_H

#include "../re/parser.h"
#include "program.h"

typedef enum {
    BRU_THOMPSON,
    BRU_GLUSHKOV,
    BRU_FLAT,
} BruConstruction;

typedef enum {
    BRU_CS_PCRE,
    BRU_CS_RE2,
} BruCaptureSemantics;

typedef enum {
    BRU_MS_NONE,
    BRU_MS_CN,
    BRU_MS_IN,
    BRU_MS_IAR,
} BruMemoScheme;

typedef struct {
    BruConstruction     construction;      /**< which construction to use     */
    int                 only_std_split;    /**< whether to only use `split`   */
    BruCaptureSemantics capture_semantics; /**< capture semantics to use      */
    BruMemoScheme       memo_scheme;       /**< memoisation scheme to use     */
    int mark_states;       /**< whether to compile state instructions         */
    int encode_priorities; /**< whether to encode priorities on transitions   */
} BruCompilerOpts;

typedef struct {
    const BruParser *parser; /**< the parser to get the regex tree            */
    BruCompilerOpts  opts;   /**< the options set for the compiler            */
} BruCompiler;

#if !defined(BRU_VM_COMPILER_DISABLE_SHORT_NAMES) && \
    (defined(BRU_VM_COMPILER_ENABLE_SHORT_NAMES) ||  \
     !defined(BRU_VM_DISABLE_SHORT_NAMES) &&         \
         (defined(BRU_VM_ENABLE_SHORT_NAMES) ||      \
          defined(BRU_ENABLE_SHORT_NAMES)))
typedef BruConstruction Construction;
#    define THOMPSON BRU_THOMPSON
#    define GLUSHKOV BRU_GLUSHKOV
#    define FLAT     BRU_FLAT

typedef BruCaptureSemantics CaptureSemantics;
#    define CS_PCRE BRU_CS_PCRE
#    define CS_RE2  BRU_CS_RE2

typedef BruMemoScheme MemoScheme;
#    define MS_NONE BRU_MS_NONE
#    define MS_CN   BRU_MS_CN
#    define MS_IN   BRU_MS_IN
#    define MS_IAR  BRU_MS_IAR

typedef BruCompilerOpts CompilerOpts;
typedef BruCompiler     Compiler;

#    define compiler_new     bru_compiler_new
#    define compiler_default bru_compiler_default
#    define compiler_free    bru_compiler_free
#    define compiler_compile bru_compiler_compile
#endif /* BRU_VM_COMPILER_ENABLE_SHORT_NAMES */

/**
 * Construct a compiler from a regex parser with specified options.
 *
 * @param[in] parser the regex parser
 * @param[in] opts   the options for the compiler
 *
 * @return the constructed compiler
 */
BruCompiler *bru_compiler_new(const BruParser      *parser,
                              const BruCompilerOpts opts);

/**
 * Construct a compiler from a regex parser with default options.
 *
 * @param[in] parser the regex parser
 *
 * @return the constructed compiler
 */
BruCompiler *bru_compiler_default(const BruParser *parser);

/**
 * Free the memory allocated for the compiler (frees the memory of the parser,
 * excluding the regex string).
 *
 * @param[in] self the compiler to free
 */
void bru_compiler_free(BruCompiler *self);

/**
 * Compile the regex tree obtained from the parser into a program.
 *
 * @param[in] self the compiler to compile
 *
 * @return the program compiled from the regex tree obtained from the parser
 */
const BruProgram *bru_compiler_compile(const BruCompiler *self);

#endif /* BRU_VM_COMPILER_H */
