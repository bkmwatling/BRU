#include <stdlib.h>
#include <string.h>

#include "../../../stc/fatp/slice.h"
#include "memory.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    bru_byte_t *memory;       /**< the extra memory (stc_slice)               */
    BruThread  *__bru_thread; /**< the underlying thread                      */
} BruThreadWithMemory;

/* --- MemoryThread function prototypes ----------------------------------- */

BRU_THREAD_FUNCTION_PROTOTYPES(memory);

/* --- Thread function definitions ------------------------------------------ */

BruThread *bru_memory_thread_new(bru_len_t memory_len, BruThread *thread)
{
    BruThreadWithMemory *ct = malloc(sizeof(*ct));
    BruThread           *t  = malloc(sizeof(*t));

    stc_slice_init(ct->memory, memory_len);
    ct->__bru_thread = thread;

    t->impl = ct;
    BRU_THREAD_SET_ALL_FUNCS(t, memory);

    return t;
}

/* --- MemoryThread function definitions ---------------------------------- */

static void memory_thread_init(void *impl, const bru_byte_t *pc, const char *sp)
{
    BruThreadWithMemory *self = impl;

    memset(self->memory, 0,
           sizeof(*self->memory) * stc_slice_len(self->memory));
    bru_thread_init(self->__bru_thread, pc, sp);
}

static int memory_thread_equal(void *t1_impl, void *t2_impl)
{
    BruThreadWithMemory *t1 = t1_impl, *t2 = t2_impl;
    size_t               i, n;

    if ((n = stc_slice_len(t1->memory)) != stc_slice_len(t2->memory))
        return FALSE;

    for (i = 0; i < n && t1->memory[i] == t2->memory[i]; i++);

    return i == n && bru_thread_equal(t1->__bru_thread, t2->__bru_thread);
}

static void memory_thread_copy(void *src_impl, void *dst_impl)
{
    BruThreadWithMemory *src = src_impl, *dst = dst_impl;

    memcpy(dst->memory, src->memory,
           sizeof(*src->memory) * stc_slice_len(src->memory));
    bru_thread_copy(src->__bru_thread, dst->__bru_thread);
}

static void memory_thread_free(void *impl)
{
    BruThreadWithMemory *self = impl;

    stc_slice_free(self->memory);
    bru_thread_free(self->__bru_thread);
}

BRU_THREAD_PC_SP_FUNCTION_PASSTHROUGH(memory,
                                      BruThreadWithMemory,
                                      __bru_thread);
