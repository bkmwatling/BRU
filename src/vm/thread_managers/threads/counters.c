#include <stdlib.h>
#include <string.h>

#include "../../../stc/fatp/slice.h"
#include "counters.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    bru_cntr_t *counters;     /**< the counter memory (stc_slice)             */
    BruThread  *__bru_thread; /**< the underlying thread                      */
} BruThreadWithMemory;

/* --- CountersThread function prototypes ----------------------------------- */

BRU_THREAD_FUNCTION_PROTOTYPES(counters);

/* --- Thread function definitions ------------------------------------------ */

BruThread *bru_counters_thread_new(bru_len_t ncounters, BruThread *thread)
{
    BruThreadWithMemory *ct = malloc(sizeof(*ct));
    BruThread           *t  = malloc(sizeof(*t));

    stc_slice_init(ct->counters, ncounters);
    ct->__bru_thread = thread;

    t->impl = ct;
    BRU_THREAD_SET_ALL_FUNCS(t, counters);

    return t;
}

/* --- CountersThread function definitions ---------------------------------- */

static void
counters_thread_init(void *impl, const bru_byte_t *pc, const char *sp)
{
    BruThreadWithMemory *self = impl;

    memset(self->counters, 0,
           sizeof(*self->counters) * stc_slice_len(self->counters));
    bru_thread_init(self->__bru_thread, pc, sp);
}

static int counters_thread_equal(void *t1_impl, void *t2_impl)
{
    BruThreadWithMemory *t1 = t1_impl, *t2 = t2_impl;
    size_t               i, n;

    if ((n = stc_slice_len(t1->counters)) != stc_slice_len(t2->counters))
        return FALSE;

    for (i = 0; i < n && t1->counters[i] == t2->counters[i]; i++);

    return i == n && bru_thread_equal(t1->__bru_thread, t2->__bru_thread);
}

static void counters_thread_copy(void *src_impl, void *dst_impl)
{
    BruThreadWithMemory *src = src_impl, *dst = dst_impl;

    memcpy(dst->counters, src->counters,
           sizeof(*src->counters) * stc_slice_len(src->counters));
    bru_thread_copy(src->__bru_thread, dst->__bru_thread);
}

static void counters_thread_free(void *impl)
{
    BruThreadWithMemory *self = impl;

    stc_slice_free(self->counters);
    bru_thread_free(self->__bru_thread);
}

BRU_THREAD_PC_SP_FUNCTION_PASSTHROUGH(counters,
                                      BruThreadWithMemory,
                                      __bru_thread);
