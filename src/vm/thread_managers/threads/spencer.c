#include <stdlib.h>

#include "../../../stc/util/utf.h"
#include "spencer.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    const bru_byte_t *pc;
    const char       *sp;
} BruSpencerThread;

/* --- SpencerThread function prototypes ----------------------------------- */

static void
spencer_thread_init(void *impl, const bru_byte_t *pc, const char *sp);
static int               spencer_thread_equal(void *t1_impl, void *t2_impl);
static void              spencer_thread_copy(void *src_impl, void *dst_impl);
static void              spencer_thread_free(void *impl);
static const bru_byte_t *spencer_thread_pc(void *impl);
static void        spencer_thread_set_pc(void *impl, const bru_byte_t *pc);
static const char *spencer_thread_sp(void *impl);
static void        spencer_thread_inc_sp(void *impl);

/* --- Thread function definitions ------------------------------------------ */

BruThread *bru_spencer_thread_new(void)
{
    BruSpencerThread *st = malloc(sizeof(*st));
    BruThread        *t  = malloc(sizeof(*t));

    t->impl = st;
    BRU_THREAD_SET_ALL_FUNCS(t, spencer);

    return t;
}

/* --- SpencerThread function definitions ---------------------------------- */

static void
spencer_thread_init(void *impl, const bru_byte_t *pc, const char *sp)
{
    BruSpencerThread *self = impl;

    self->pc = pc;
    self->sp = sp;
}

static int spencer_thread_equal(void *t1_impl, void *t2_impl)
{
    BruSpencerThread *t1 = t1_impl, *t2 = t2_impl;
    return t1->pc == t2->pc;
}

static void spencer_thread_copy(void *src_impl, void *dst_impl)
{
    BruSpencerThread *src = src_impl, *dst = dst_impl;

    *dst = *src;
}

static void spencer_thread_free(void *impl) { free(impl); }

static const bru_byte_t *spencer_thread_pc(void *impl)
{
    BruSpencerThread *self = impl;
    return self->pc;
}

static void spencer_thread_set_pc(void *impl, const bru_byte_t *pc)
{
    BruSpencerThread *self = impl;
    self->pc               = pc;
}

static const char *spencer_thread_sp(void *impl)
{
    BruSpencerThread *self = impl;
    return self->sp;
}

static void spencer_thread_inc_sp(void *impl)
{
    BruSpencerThread *self = impl;
    self->sp               = stc_utf8_str_next(self->sp);
}
