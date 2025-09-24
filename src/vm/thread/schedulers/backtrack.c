#include <stdbool.h>
#include <stdlib.h>

#include <bru/vm/thread/schedulers/backtrack.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    size_t              in_order_idx; /**< index to insert threads in-order   */
    BruThread          *active;       /**< active thread for the scheduler    */
    StcVec(BruThread *) stack;        /**< thread stack for DFS scheduling    */
} BruBacktrackThreadScheduler;

/* --- BacktrackScheduler function prototypes ------------------------------- */

static void       backtrack_ts_init(void *impl);
static bool       backtrack_ts_schedule(void *impl, BruThread *thread);
static bool       backtrack_ts_schedule_in_order(void *impl, BruThread *thread);
static bool       backtrack_ts_has_next(const void *impl);
static BruThread *backtrack_ts_next(void *impl);
static void       backtrack_ts_free(void *impl);

BruThreadScheduler *bru_backtrack_ts_new(void)
{
    BruBacktrackThreadScheduler *bs = malloc(sizeof(*bs));
    BruThreadScheduler          *s  = malloc(sizeof(*s));

    bs->in_order_idx = 0;
    bs->active       = NULL;
    stc_vec_default_init(&bs->stack);

    s->impl              = bs;
    s->init              = backtrack_ts_init;
    s->schedule          = backtrack_ts_schedule;
    s->schedule_in_order = backtrack_ts_schedule_in_order;
    s->has_next          = backtrack_ts_has_next;
    s->next              = backtrack_ts_next;
    s->free              = backtrack_ts_free;

    return s;
}

/* --- BacktrackScheduler function definitions ------------------------------ */

static void backtrack_ts_init(void *impl)
{
    BruBacktrackThreadScheduler *self = impl;

    self->in_order_idx = 0;
    self->active       = NULL;
}

static bool backtrack_ts_schedule(void *impl, BruThread *thread)
{
    BruBacktrackThreadScheduler *self = impl;
    self->in_order_idx                = stc_vec_len(self->stack) + 1;
    if (self->active)
        stc_vec_push_back(&self->stack, thread);
    else
        self->active = thread;
    return true;
}

static bool backtrack_ts_schedule_in_order(void *impl, BruThread *thread)
{
    BruBacktrackThreadScheduler *self = impl;
    size_t                       len  = stc_vec_len(self->stack);

    if (self->in_order_idx > len) {
        backtrack_ts_schedule(self, thread);
        self->in_order_idx = len;
    } else if (self->in_order_idx == len) {
        stc_vec_push_back(&self->stack, thread);
    } else {
        stc_vec_insert(&self->stack, self->in_order_idx, thread);
    }

    return true;
}

static bool backtrack_ts_has_next(const void *impl)
{
    const BruBacktrackThreadScheduler *self = impl;

    return self->active != NULL || !stc_vec_is_empty(self->stack);
}

static BruThread *backtrack_ts_next(void *impl)
{
    BruBacktrackThreadScheduler *self   = impl;
    BruThread                   *thread = self->active;

    self->in_order_idx = stc_vec_len(self->stack) + 1;
    self->active       = NULL;
    if (thread == NULL && !stc_vec_is_empty(self->stack))
        thread = stc_vec_pop_back(&self->stack);

    return thread;
}

static void backtrack_ts_free(void *impl)
{
    BruBacktrackThreadScheduler *self = impl;
    stc_vec_free(self->stack);
    free(self);
}
