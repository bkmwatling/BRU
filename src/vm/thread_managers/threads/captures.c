#include <stdlib.h>
#include <string.h>

#include "../../../stc/fatp/slice.h"
#include "captures.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    const char **captures;     /**< the capture memory (stc_slice)            */
    BruThread   *__bru_thread; /**< the underlying thread                     */
} BruThreadWithCaptures;

/* --- CapturesThread function prototypes ----------------------------------- */

BRU_THREAD_FUNCTION_PROTOTYPES(captures);

/* --- Thread function definitions ------------------------------------------ */

BruThread *bru_captures_thread_new(bru_len_t ncaptures, BruThread *thread)
{
    BruThreadWithCaptures *ct = malloc(sizeof(*ct));
    BruThread             *t  = malloc(sizeof(*t));

    stc_slice_init(ct->captures, 2 * ncaptures);
    ct->__bru_thread = thread;

    t->impl = ct;
    BRU_THREAD_SET_ALL_FUNCS(t, captures);

    return t;
}

/* --- CapturesThread function definitions ---------------------------------- */

static void
captures_thread_init(void *impl, const bru_byte_t *pc, const char *sp)
{
    BruThreadWithCaptures *self = impl;

    memset(self->captures, 0,
           sizeof(*self->captures) * stc_slice_len(self->captures));
    bru_thread_init(self->__bru_thread, pc, sp);
}

static int captures_thread_equal(void *t1_impl, void *t2_impl)
{
    BruThreadWithCaptures *t1 = t1_impl, *t2 = t2_impl;
    size_t                 i, n;

    if ((n = stc_slice_len(t1->captures)) != stc_slice_len(t2->captures))
        return FALSE;

    for (i = 0; i < n && t1->captures[i] == t2->captures[i]; i++);

    return i == n && bru_thread_equal(t1->__bru_thread, t2->__bru_thread);
}

static void captures_thread_copy(void *src_impl, void *dst_impl)
{
    BruThreadWithCaptures *src = src_impl, *dst = dst_impl;

    memcpy(dst->captures, src->captures,
           sizeof(*src->captures) * stc_slice_len(src->captures));
    bru_thread_copy(src->__bru_thread, dst->__bru_thread);
}

static void captures_thread_free(void *impl)
{
    BruThreadWithCaptures *self = impl;

    stc_slice_free(self->captures);
    bru_thread_free(self->__bru_thread);
}

BRU_THREAD_PC_SP_FUNCTION_PASSTHROUGH(captures,
                                      BruThreadWithCaptures,
                                      __bru_thread);
