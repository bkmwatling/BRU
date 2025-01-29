#include <stdlib.h>
#include <string.h>

#include <bru/vm/thread_managers/benchmark.h>

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

static void benchmark_thread_manager_free(BruThreadManager *tm);

static BruThread *benchmark_thread_manager_alloc_thread(BruThreadManager *tm);
static BruThread *benchmark_thread_manager_next_thread(BruThreadManager *tm);
static void       benchmark_thread_manager_kill_thread(BruThreadManager *tm,
                                                       BruThread        *t);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_benchmark_thread_manager_new(BruThreadManager *tm,
                                                   FILE             *logfile)
{
    BruBenchmarkThreadManager *btm = malloc(sizeof(*btm));
    BruThreadManagerInterface *tmi, *super;

    btm->logfile            = logfile ? logfile : stderr;
    btm->thread_alloc_count = 0;
    btm->prev_thread        = NULL;

    memset(btm->inst_counts, 0, sizeof(btm->inst_counts));

    super     = bru_vt_curr(tm);
    tmi       = bru_thread_manager_interface_new(btm, super->_thread_size);
    tmi->free = benchmark_thread_manager_free;
    tmi->next_thread  = benchmark_thread_manager_next_thread;
    tmi->kill_thread  = benchmark_thread_manager_kill_thread;
    tmi->alloc_thread = benchmark_thread_manager_alloc_thread;

    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BenchmarkThreadManager function defintions --------------------------- */

static void benchmark_thread_manager_free(BruThreadManager *tm)
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

static BruThread *benchmark_thread_manager_alloc_thread(BruThreadManager *tm)
{
    BruBenchmarkThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);
    BruThread                 *t;

    bru_vt_call_super_function(tm, tmi, t, alloc_thread);
    if (t) self->thread_alloc_count++;
    return t;
}

static BruThread *benchmark_thread_manager_next_thread(BruThreadManager *tm)
{
    BruBenchmarkThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);
    const bru_byte_t          *_pc;

    if (bru_vt_call_super_function(tm, tmi, self->prev_thread, next_thread))
        INC_INST_COUNT(self,
                       *bru_thread_manager_pc(tm, _pc, self->prev_thread));

    return self->prev_thread;
}

static void benchmark_thread_manager_kill_thread(BruThreadManager *tm,
                                                 BruThread        *t)
{
    BruBenchmarkThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);
    const bru_byte_t          *_pc;

    if (t == self->prev_thread)
        INC_INST_FAIL_COUNT(self, *bru_thread_manager_pc(tm, _pc, t));
    bru_vt_call_super_procedure(tm, tmi, kill_thread, t);
}

#undef INST_COUNT_LEN
#undef INST_IDX
#undef INST_COUNT
#undef INC_INST_COUNT
#undef INST_FAIL_COUNT
#undef INC_INST_FAIL_COUNT
#undef LOG_INSTS
