#include <stdlib.h>
#include <string.h>

#include <bru/vm/thread/managers/benchmark.h>

#define INST_COUNT_LEN                  (2 * BRU_NBYTECODES)
#define INST_IDX(inst)                  (2 * (BruBytecode) (inst))
#define INST_COUNT(self, inst)          (self)->inst_counts[INST_IDX(inst)]
#define INC_INST_COUNT(self, inst)      INST_COUNT(self, inst)++
#define INST_FAIL_COUNT(self, inst)     (self)->inst_counts[INST_IDX(inst) + 1]
#define INC_INST_FAIL_COUNT(self, inst) INST_FAIL_COUNT(self, inst)++

typedef struct {
    FILE *logfile; /**< the log file stream to print benchmark information to */

    // TODO: use pointers to facilitate shared counting when cloning
    BruThread *prev_thread; /**< previous thread returned from next_thread    */
    size_t     thread_alloc_count; /**< the number of spawned threads         */
    size_t     inst_counts[INST_COUNT_LEN]; /**< instruction execution counts */
} BruBenchmarkThreadManager;

/* --- BenchmarkThreadManager function prototypes --------------------------- */

static void benchmark_tm_free(BruThreadManager *tm);

static BruThread *benchmark_tm_alloc_thread(BruThreadManager *tm);
static BruThread *benchmark_tm_next_thread(BruThreadManager *tm);
static void       benchmark_tm_kill_thread(BruThreadManager *tm, BruThread *t);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_benchmark_tm_new(BruThreadManager *tm, FILE *logfile)
{
    BruBenchmarkThreadManager *btm = malloc(sizeof(*btm));
    BruThreadManagerInterface *tmi, *super;

    btm->logfile            = logfile ? logfile : stderr;
    btm->thread_alloc_count = 0;
    btm->prev_thread        = NULL;

    memset(btm->inst_counts, 0, sizeof(btm->inst_counts));

    super             = bru_vt_curr(tm);
    tmi               = bru_tmi_new(btm, super->_thread_size);
    tmi->free         = benchmark_tm_free;
    tmi->next_thread  = benchmark_tm_next_thread;
    tmi->kill_thread  = benchmark_tm_kill_thread;
    tmi->alloc_thread = benchmark_tm_alloc_thread;

    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BenchmarkThreadManager function defintions --------------------------- */

static void benchmark_tm_free(BruThreadManager *tm)
{
    BruBenchmarkThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);

#define LOG_INST(i)                                                         \
    fprintf(self->logfile, #i ": %lu (FAILED: %lu)\n", INST_COUNT(self, i), \
            INST_FAIL_COUNT(self, i));

    BRU_FOR_LIST_OF_INSTRUCTIONS(LOG_INST);
    fprintf(self->logfile, "THREAD_ALLOC_COUNT: %zu\n",
            self->thread_alloc_count);

    free(self);

    bru_vt_call_super_procedure(tm, tmi, free);
}

static BruThread *benchmark_tm_alloc_thread(BruThreadManager *tm)
{
    BruBenchmarkThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);
    BruThread *thread = bru_vt_call_super_function(tm, tmi, alloc_thread);

    if (thread) self->thread_alloc_count++;
    return thread;
}

static BruThread *benchmark_tm_next_thread(BruThreadManager *tm)
{
    BruBenchmarkThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);

    if ((self->prev_thread = bru_vt_call_super_function(tm, tmi, next_thread)))
        INC_INST_COUNT(self, *bru_tm_pc(tm, self->prev_thread));

    return self->prev_thread;
}

static void benchmark_tm_kill_thread(BruThreadManager *tm, BruThread *t)
{
    BruBenchmarkThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);

    if (t == self->prev_thread) INC_INST_FAIL_COUNT(self, *bru_tm_pc(tm, t));
    bru_vt_call_super_procedure(tm, tmi, kill_thread, t);
}

#undef INST_COUNT_LEN
#undef INST_IDX
#undef INST_COUNT
#undef INC_INST_COUNT
#undef INST_FAIL_COUNT
#undef INC_INST_FAIL_COUNT
#undef LOG_INSTS
