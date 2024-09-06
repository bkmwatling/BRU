#include <stdlib.h>
#include <string.h>

#include "benchmark.h"

#define INST_COUNT_LEN               (2 * BRU_NBYTECODES)
#define INST_IDX(i)                  (2 * (i))
#define INST_COUNT(self, i)          (self)->inst_counts[INST_IDX(i)]
#define INC_INST_COUNT(self, i)      INST_COUNT(self, i)++
#define INST_FAIL_COUNT(self, i)     (self)->inst_counts[INST_IDX(i) + 1]
#define INC_INST_FAIL_COUNT(self, i) INST_FAIL_COUNT(self, i)++
#define CURR_INST(t)                 *(t)->pc
#define INC_CURR_INST(self, t)       INC_INST_COUNT(self, CURR_INST(t))
#define INC_CURR_INST_FAIL(self, t)  INC_INST_FAIL_COUNT(self, CURR_INST(t))
#define LOG_INSTS(self, i)                                                    \
    fprintf((self)->logfile, #i ": %lu (FAILED: %lu)\n", INST_COUNT(self, i), \
            INST_FAIL_COUNT(self, i))

typedef struct {
    FILE *logfile; /**< the log file stream to print benchmark information to */

    // TODO: use pointers to facilitate shared counting when cloning
    size_t spawn_count;                 /**< the number of spawned threads    */
    size_t inst_counts[INST_COUNT_LEN]; /**< instruction execution counts     */

    BruThreadManager *__manager; /**< the thread manager being wrapped        */
} BruBenchmarkThreadManager;

/* --- BenchmarkThreadManager function prototypes --------------------------- */

static void benchmark_thread_manager_init(void             *impl,
                                          const bru_byte_t *start_pc,
                                          const char       *start_sp);
static void benchmark_thread_manager_reset(void *impl);
static void benchmark_thread_manager_free(void *impl);
static int  benchmark_thread_manager_done_exec(void *impl);

static void benchmark_thread_manager_schedule_thread(void *impl, BruThread *t);
static void benchmark_thread_manager_schedule_thread_in_order(void      *impl,
                                                              BruThread *t);
static BruThread *benchmark_thread_manager_next_thread(void *impl);
static void       benchmark_thread_manager_notify_thread_match(void      *impl,
                                                               BruThread *t);
static BruThread *benchmark_thread_manager_clone_thread(void            *impl,
                                                        const BruThread *t);
static void benchmark_thread_manager_kill_thread(void *impl, BruThread *t);
static void benchmark_thread_manager_init_memoisation(void       *impl,
                                                      size_t      nmemo_insts,
                                                      const char *text);

static const bru_byte_t *benchmark_thread_pc(void *impl, const BruThread *t);
static void
benchmark_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc);
static const char *benchmark_thread_sp(void *impl, const BruThread *t);
static void        benchmark_thread_inc_sp(void *impl, BruThread *t);
static int benchmark_thread_memoise(void *impl, BruThread *t, bru_len_t idx);
static bru_cntr_t
benchmark_thread_counter(void *impl, const BruThread *t, bru_len_t idx);
static void benchmark_thread_set_counter(void      *impl,
                                         BruThread *t,
                                         bru_len_t  idx,
                                         bru_cntr_t val);
static void
benchmark_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx);
static void *
benchmark_thread_memory(void *impl, const BruThread *t, bru_len_t idx);
static void benchmark_thread_set_memory(void       *impl,
                                        BruThread  *t,
                                        bru_len_t   idx,
                                        const void *val,
                                        size_t      size);
static const char *const *
benchmark_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures);
static void
benchmark_thread_set_capture(void *impl, BruThread *t, bru_len_t idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *
bru_benchmark_thread_manager_new(BruThreadManager *thread_manager,
                                 FILE             *logfile)
{
    BruBenchmarkThreadManager *btm = malloc(sizeof(*btm));
    BruThreadManager          *tm  = malloc(sizeof(*tm));

    btm->logfile     = logfile ? logfile : stderr;
    btm->spawn_count = 0;
    memset(btm->inst_counts, 0, sizeof(btm->inst_counts));
    btm->__manager = thread_manager;

    BRU_THREAD_MANAGER_SET_ALL_FUNCS(tm, benchmark);
    tm->impl = btm;

    return tm;
}

/* --- BenchmarkThreadManager function defintions --------------------------- */

static void benchmark_thread_manager_init(void             *impl,
                                          const bru_byte_t *start_pc,
                                          const char       *start_sp)
{
    bru_thread_manager_init(((BruBenchmarkThreadManager *) impl)->__manager,
                            start_pc, start_sp);
}

static void benchmark_thread_manager_reset(void *impl)
{
    BruBenchmarkThreadManager *self = impl;
    bru_thread_manager_reset(self->__manager);
    // TODO: reset benchmark instrumentation?
    // self->spawn_count = 0;
    // memset(self->inst_counts, 0, sizeof(self->inst_counts));
}

static void benchmark_thread_manager_free(void *impl)
{
    BruBenchmarkThreadManager *self = impl;

    LOG_INSTS(self, BRU_MATCH);
    LOG_INSTS(self, BRU_MEMO);
    LOG_INSTS(self, BRU_CHAR);
    LOG_INSTS(self, BRU_PRED);
    LOG_INSTS(self, BRU_STATE);

    bru_thread_manager_free(self->__manager);
    free(impl);
}

static int benchmark_thread_manager_done_exec(void *impl)
{
    return bru_thread_manager_done_exec(
        ((BruBenchmarkThreadManager *) impl)->__manager);
}

static void benchmark_thread_manager_schedule_thread(void *impl, BruThread *t)
{
    bru_thread_manager_schedule_thread(
        ((BruBenchmarkThreadManager *) impl)->__manager, t);
}

static void benchmark_thread_manager_schedule_thread_in_order(void      *impl,
                                                              BruThread *t)
{
    bru_thread_manager_schedule_thread_in_order(
        ((BruBenchmarkThreadManager *) impl)->__manager, t);
}

static BruThread *benchmark_thread_manager_next_thread(void *impl)
{
    BruBenchmarkThreadManager *self = impl;
    BruThread *t = bru_thread_manager_next_thread(self->__manager);

    if (t) INC_CURR_INST(self, t);

    return t;
}

static void benchmark_thread_manager_notify_thread_match(void      *impl,
                                                         BruThread *t)
{
    bru_thread_manager_notify_thread_match(
        ((BruBenchmarkThreadManager *) impl)->__manager, t);
}

static BruThread *benchmark_thread_manager_clone_thread(void            *impl,
                                                        const BruThread *t)
{
    return bru_thread_manager_clone_thread(
        ((BruBenchmarkThreadManager *) impl)->__manager, t);
}

static void benchmark_thread_manager_kill_thread(void *impl, BruThread *t)
{
    BruBenchmarkThreadManager *self = impl;

    INC_CURR_INST_FAIL(self, t);
    bru_thread_manager_kill_thread(self->__manager, t);
}

static const bru_byte_t *benchmark_thread_pc(void *impl, const BruThread *t)
{
    return bru_thread_manager_pc(
        ((BruBenchmarkThreadManager *) impl)->__manager, t);
}

static void
benchmark_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc)
{
    bru_thread_manager_set_pc(((BruBenchmarkThreadManager *) impl)->__manager,
                              t, pc);
}

static const char *benchmark_thread_sp(void *impl, const BruThread *t)
{
    return bru_thread_manager_sp(
        ((BruBenchmarkThreadManager *) impl)->__manager, t);
}

static void benchmark_thread_inc_sp(void *impl, BruThread *t)
{
    bru_thread_manager_inc_sp(((BruBenchmarkThreadManager *) impl)->__manager,
                              t);
}

static void benchmark_thread_manager_init_memoisation(void       *impl,
                                                      size_t      nmemo_insts,
                                                      const char *text)
{
    bru_thread_manager_init_memoisation(
        ((BruBenchmarkThreadManager *) impl)->__manager, nmemo_insts, text);
}

static int benchmark_thread_memoise(void *impl, BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_memoise(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, idx);
}

static bru_cntr_t
benchmark_thread_counter(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_counter(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, idx);
}

static void benchmark_thread_set_counter(void      *impl,
                                         BruThread *t,
                                         bru_len_t  idx,
                                         bru_cntr_t val)
{
    bru_thread_manager_set_counter(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, idx, val);
}

static void
benchmark_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_inc_counter(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, idx);
}

static void *
benchmark_thread_memory(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_memory(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, idx);
}

static void benchmark_thread_set_memory(void       *impl,
                                        BruThread  *t,
                                        bru_len_t   idx,
                                        const void *val,
                                        size_t      size)
{
    bru_thread_manager_set_memory(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, idx, val, size);
}

static const char *const *
benchmark_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures)
{
    return bru_thread_manager_captures(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, ncaptures);
}

static void
benchmark_thread_set_capture(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_set_capture(
        ((BruBenchmarkThreadManager *) impl)->__manager, t, idx);
}

#undef INST_COUNT_LEN
#undef DECL_INST_COUNT
#undef INST_IDX
#undef INST_COUNT
#undef INC_INST_COUNT
#undef INST_FAILURE_COUNT
#undef INC_INST_FAILURE_COUNT
#undef INC_CURR_INST
#undef LOG_INSTS
