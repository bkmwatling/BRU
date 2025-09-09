#include <stdlib.h>

#include <bru/vm/thread/schedulers/backtrack.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_backtrack_scheduler {
    size_t              in_order_idx; /**< index to insert threads in-order   */
    BruThread          *active;       /**< active thread for the scheduler    */
    StcVec(BruThread *) stack;        /**< thread stack for DFS scheduling    */
} BruBacktrackScheduler;

/* --- BacktrackScheduler function prototypes ------------------------------- */

static void backtrack_scheduler_init(void *impl);
static int  backtrack_scheduler_schedule(void *impl, BruThread *thread);
static int backtrack_scheduler_schedule_in_order(void *impl, BruThread *thread);
static int backtrack_scheduler_has_next(const void *impl);
static BruThread *backtrack_scheduler_next(void *impl);
static void       backtrack_scheduler_free(void *impl);

BruScheduler *bru_backtrack_scheduler_new(void)
{
    BruBacktrackScheduler *bs = malloc(sizeof(*bs));
    BruScheduler          *s  = malloc(sizeof(*s));

    bs->in_order_idx = 0;
    bs->active       = NULL;
    stc_vec_default_init(&bs->stack);

    s->impl              = bs;
    s->init              = backtrack_scheduler_init;
    s->schedule          = backtrack_scheduler_schedule;
    s->schedule_in_order = backtrack_scheduler_schedule_in_order;
    s->has_next          = backtrack_scheduler_has_next;
    s->next              = backtrack_scheduler_next;
    s->free              = backtrack_scheduler_free;

    return s;
}

/* --- BacktrackScheduler function definitions ------------------------------ */

static void backtrack_scheduler_init(void *impl)
{
    BruBacktrackScheduler *self = impl;

    self->in_order_idx = 0;
    self->active       = NULL;
}

static int backtrack_scheduler_schedule(void *impl, BruThread *thread)
{
    BruBacktrackScheduler *self = impl;
    self->in_order_idx          = stc_vec_len(self->stack) + 1;
    if (self->active)
        stc_vec_push_back(&self->stack, thread);
    else
        self->active = thread;
    return TRUE;
}

static int backtrack_scheduler_schedule_in_order(void *impl, BruThread *thread)
{
    BruBacktrackScheduler *self = impl;
    size_t                 len  = stc_vec_len(self->stack);

    if (self->in_order_idx > len) {
        backtrack_scheduler_schedule(self, thread);
        self->in_order_idx = len;
    } else if (self->in_order_idx == len) {
        stc_vec_push_back(&self->stack, thread);
    } else {
        stc_vec_insert(&self->stack, self->in_order_idx, thread);
    }

    return TRUE;
}

static int backtrack_scheduler_has_next(const void *impl)
{
    const BruBacktrackScheduler *self = impl;

    return self->active != NULL || !stc_vec_is_empty(self->stack);
}

static BruThread *backtrack_scheduler_next(void *impl)
{
    BruBacktrackScheduler *self   = impl;
    BruThread             *thread = self->active;

    self->in_order_idx = stc_vec_len(self->stack) + 1;
    self->active       = NULL;
    if (thread == NULL && !stc_vec_is_empty(self->stack))
        thread = stc_vec_pop_back(&self->stack);

    return thread;
}

static void backtrack_scheduler_free(void *impl)
{
    BruBacktrackScheduler *self = impl;
    stc_vec_free(self->stack);
    free(self);
}
