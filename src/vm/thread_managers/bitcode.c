#include <stdlib.h>

#include "../../stc/fatp/vec.h"

#include "bitcode.h"

#define FIND_CODE_VEC(codes, thread, i)                  \
    do {                                                 \
        for ((i) = 0; (i) < stc_vec_len(codes); (i)++) { \
            if ((codes)[i].thread_ptr == thread) break;  \
        }                                                \
    } while (0)

#define CREATE_CODE_VEC(codes, thread, i)                         \
    do {                                                          \
        FIND_CODE_VEC(codes, thread, i);                          \
        if ((i) < stc_vec_len(codes)) break;                      \
        stc_vec_push_back((codes), (ThreadCode){ 0 });            \
        stc_vec_last(codes).thread_ptr = (thread);                \
        stc_vec_last(codes).bits =                                \
            stc_vec_default(sizeof(*(stc_vec_last(codes).bits))); \
    } while (0)

#define DELETE_CODE_VEC(codes, i)                               \
    do {                                                        \
        if ((i) < stc_vec_len(codes)) stc_vec_remove(codes, i); \
    } while (0)

#define WRITE_BIT_VEC(threadcode, bit)           \
    do {                                         \
        stc_vec_push_back(threadcode.bits, bit); \
    } while (0)

#define FIND_CODE   FIND_CODE_VEC
#define CREATE_CODE CREATE_CODE_VEC
#define DELETE_CODE DELETE_CODE_VEC
#define WRITE_BIT   WRITE_BIT_VEC

typedef struct {
    BruThread *thread_ptr; /**< thread pointer                                */
    char      *bits;       /**< stc_vec of bits                               */
} ThreadCode;

typedef struct {
    FILE       *logfile; /**< the stream to print bitcode information to      */
    ThreadCode *codes;   /**< stc_vec of ThreadCodes                          */

    BruThreadManager *__manager; /**< the thread manager being wrapped        */
} BruBitcodeThreadManager;

/* --- BitcodeThreadManager function prototypes ----------------------------- */

static void bitcode_thread_manager_init(void             *impl,
                                        const bru_byte_t *start_pc,
                                        const char       *start_sp);
static void bitcode_thread_manager_reset(void *impl);
static void bitcode_thread_manager_free(void *impl);
static int  bitcode_thread_manager_done_exec(void *impl);

static void bitcode_thread_manager_schedule_thread(void *impl, BruThread *t);
static void bitcode_thread_manager_schedule_thread_in_order(void      *impl,
                                                            BruThread *t);
static BruThread *bitcode_thread_manager_next_thread(void *impl);
static void       bitcode_thread_manager_notify_thread_match(void      *impl,
                                                             BruThread *t);
static BruThread *bitcode_thread_manager_clone_thread(void            *impl,
                                                      const BruThread *t);
static void       bitcode_thread_manager_kill_thread(void *impl, BruThread *t);
static void       bitcode_thread_manager_init_memoisation(void       *impl,
                                                          size_t      nmemo,
                                                          const char *text);

static const bru_byte_t *bitcode_thread_pc(void *impl, const BruThread *t);
static void
bitcode_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc);
static const char *bitcode_thread_sp(void *impl, const BruThread *t);
static void        bitcode_thread_inc_sp(void *impl, BruThread *t);
static int bitcode_thread_memoise(void *impl, BruThread *t, bru_len_t idx);
static bru_cntr_t
bitcode_thread_counter(void *impl, const BruThread *t, bru_len_t idx);
static void bitcode_thread_set_counter(void      *impl,
                                       BruThread *t,
                                       bru_len_t  idx,
                                       bru_cntr_t val);
static void bitcode_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx);
static void *
bitcode_thread_memory(void *impl, const BruThread *t, bru_len_t idx);
static void bitcode_thread_set_memory(void       *impl,
                                      BruThread  *t,
                                      bru_len_t   idx,
                                      const void *val,
                                      size_t      size);
static const char *const *
bitcode_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures);
static void bitcode_thread_set_capture(void *impl, BruThread *t, bru_len_t idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *
bru_bitcode_thread_manager_new(BruThreadManager *thread_manager, FILE *logfile)
{
    BruBitcodeThreadManager *btm = malloc(sizeof(*btm));
    BruThreadManager        *tm  = malloc(sizeof(*tm));

    btm->logfile   = logfile ? logfile : stderr;
    btm->codes     = stc_vec_default(sizeof(*(btm->codes)));
    btm->__manager = thread_manager;

    BRU_THREAD_MANAGER_SET_ALL_FUNCS(tm, bitcode);
    tm->impl = btm;

    return tm;
}

/* --- BitcodeThreadManager function defintions --------------------------- */

static void bitcode_thread_manager_init(void             *impl,
                                        const bru_byte_t *start_pc,
                                        const char       *start_sp)
{
    bru_thread_manager_init(((BruBitcodeThreadManager *) impl)->__manager,
                            start_pc, start_sp);
}

static void bitcode_thread_manager_reset(void *impl)
{
    BruBitcodeThreadManager *self = impl;
    bru_thread_manager_reset(self->__manager);
    // TODO: reset bitcode instrumentation?
    // self->spawn_count = 0;
    // memset(self->inst_counts, 0, sizeof(self->inst_counts));
}

static void bitcode_thread_manager_free(void *impl)
{
    BruBitcodeThreadManager *self = impl;
    bru_thread_manager_free(self->__manager);
    free(impl);
}

static int bitcode_thread_manager_done_exec(void *impl)
{
    return bru_thread_manager_done_exec(
        ((BruBitcodeThreadManager *) impl)->__manager);
}

static void bitcode_thread_manager_schedule_thread(void *impl, BruThread *t)
{
    bru_thread_manager_schedule_thread(
        ((BruBitcodeThreadManager *) impl)->__manager, t);
}

static void bitcode_thread_manager_schedule_thread_in_order(void      *impl,
                                                            BruThread *t)
{
    bru_thread_manager_schedule_thread_in_order(
        ((BruBitcodeThreadManager *) impl)->__manager, t);
}

static BruThread *bitcode_thread_manager_next_thread(void *impl)
{
    BruBitcodeThreadManager *self = impl;
    BruThread *t = bru_thread_manager_next_thread(self->__manager);
    int        i;
    char       bit;
    switch (*t->pc) {
        case BRU_WRITE0: bit = '0'; break;
        case BRU_WRITE1: bit = '1'; break;
        default: goto done;
    }

    CREATE_CODE(self->codes, t, i);
    WRITE_BIT(self->codes[i], bit);

done:
    return t;
}

static void bitcode_thread_manager_notify_thread_match(void *impl, BruThread *t)
{
    bru_thread_manager_notify_thread_match(
        ((BruBitcodeThreadManager *) impl)->__manager, t);
}

static BruThread *bitcode_thread_manager_clone_thread(void            *impl,
                                                      const BruThread *t)
{
    return bru_thread_manager_clone_thread(
        ((BruBitcodeThreadManager *) impl)->__manager, t);
}

static void bitcode_thread_manager_kill_thread(void *impl, BruThread *t)
{
    BruBitcodeThreadManager *self = impl;
    int                      i;

    FIND_CODE(self->codes, t, i);
    DELETE_CODE(self->codes, i);
    bru_thread_manager_kill_thread(self->__manager, t);
}

static const bru_byte_t *bitcode_thread_pc(void *impl, const BruThread *t)
{
    return bru_thread_manager_pc(((BruBitcodeThreadManager *) impl)->__manager,
                                 t);
}

static void
bitcode_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc)
{
    bru_thread_manager_set_pc(((BruBitcodeThreadManager *) impl)->__manager, t,
                              pc);
}

static const char *bitcode_thread_sp(void *impl, const BruThread *t)
{
    return bru_thread_manager_sp(((BruBitcodeThreadManager *) impl)->__manager,
                                 t);
}

static void bitcode_thread_inc_sp(void *impl, BruThread *t)
{
    bru_thread_manager_inc_sp(((BruBitcodeThreadManager *) impl)->__manager, t);
}

static void bitcode_thread_manager_init_memoisation(void       *impl,
                                                    size_t      nmemo_insts,
                                                    const char *text)
{
    bru_thread_manager_init_memoisation(
        ((BruBitcodeThreadManager *) impl)->__manager, nmemo_insts, text);
}

static int bitcode_thread_memoise(void *impl, BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_memoise(
        ((BruBitcodeThreadManager *) impl)->__manager, t, idx);
}

static bru_cntr_t
bitcode_thread_counter(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_counter(
        ((BruBitcodeThreadManager *) impl)->__manager, t, idx);
}

static void bitcode_thread_set_counter(void      *impl,
                                       BruThread *t,
                                       bru_len_t  idx,
                                       bru_cntr_t val)
{
    bru_thread_manager_set_counter(
        ((BruBitcodeThreadManager *) impl)->__manager, t, idx, val);
}

static void bitcode_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_inc_counter(
        ((BruBitcodeThreadManager *) impl)->__manager, t, idx);
}

static void *
bitcode_thread_memory(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_memory(
        ((BruBitcodeThreadManager *) impl)->__manager, t, idx);
}

static void bitcode_thread_set_memory(void       *impl,
                                      BruThread  *t,
                                      bru_len_t   idx,
                                      const void *val,
                                      size_t      size)
{
    bru_thread_manager_set_memory(((BruBitcodeThreadManager *) impl)->__manager,
                                  t, idx, val, size);
}

static const char *const *
bitcode_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures)
{
    return bru_thread_manager_captures(
        ((BruBitcodeThreadManager *) impl)->__manager, t, ncaptures);
}

static void bitcode_thread_set_capture(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_set_capture(
        ((BruBitcodeThreadManager *) impl)->__manager, t, idx);
}
