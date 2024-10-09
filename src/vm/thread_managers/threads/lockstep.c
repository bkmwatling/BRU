#include <stdlib.h>

#include "../../../utils.h"
#include "lockstep.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_thompson_thread {
    const bru_byte_t *pc;
    const char      **sp;
} BruLockstepThread;

/* --- LockstepThread function prototypes ----------------------------------- */

static void
lockstep_thread_init(void *impl, const bru_byte_t *pc, const char *sp);
static int               lockstep_thread_equal(void *t1_impl, void *t2_impl);
static void              lockstep_thread_copy(void *src_impl, void *dst_impl);
static void              lockstep_thread_free(void *impl);
static const bru_byte_t *lockstep_thread_pc(void *impl);
static void        lockstep_thread_set_pc(void *impl, const bru_byte_t *pc);
static const char *lockstep_thread_sp(void *impl);
static void        lockstep_thread_inc_sp(void *impl);

/* --- Thread function definitions ------------------------------------------ */

BruThread *bru_lockstep_thread_new(void)
{
    BruLockstepThread *lt = malloc(sizeof(*lt));
    BruThread         *t  = malloc(sizeof(*t));

    lt->sp = malloc(sizeof(*lt->sp));

    t->impl = lt;
    BRU_THREAD_SET_ALL_FUNCS(t, lockstep);

    return t;
}

/* --- LockstepThread function definitions ---------------------------------- */

static void
lockstep_thread_init(void *impl, const bru_byte_t *pc, const char *sp)
{
    BruLockstepThread *self = impl;

    self->pc  = pc;
    // XXX: Here be dragons
    *self->sp = sp;
}

static int lockstep_thread_equal(void *t1_impl, void *t2_impl)
{
    BruLockstepThread *t1 = t1_impl, *t2 = t2_impl;
    return t1->pc == t2->pc;
}

static void lockstep_thread_copy(void *src_impl, void *dst_impl)
{
    BruLockstepThread *src = src_impl, *dst = dst_impl;

    dst->pc  = src->pc;
    *dst->sp = *src->sp;
}

static void lockstep_thread_free(void *impl)
{
    BruLockstepThread *self = impl;

    free(self->sp);
    free(self);
}

static const bru_byte_t *lockstep_thread_pc(void *impl)
{
    BruLockstepThread *self = impl;
    return self->pc;
}

static void lockstep_thread_set_pc(void *impl, const bru_byte_t *pc)
{
    BruLockstepThread *self = impl;
    self->pc                = pc;
}

static const char *lockstep_thread_sp(void *impl)
{
    BruLockstepThread *self = impl;
    return *self->sp;
}

static void lockstep_thread_inc_sp(void *impl) { BRU_UNUSED(impl); }
