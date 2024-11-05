#include <stdlib.h>

#include "spencer.h"

/* --- Type definitions ----------------------------------------------------- */

typedef struct bru_spencer_scheduler {
    size_t              in_order_idx; /**< index to insert threads in-order   */
    BruThread          *active;       /**< active thread for the scheduler    */
    StcVec(BruThread *) stack;        /**< thread stack for DFS scheduling    */
} BruSpencerScheduler;

/* --- Scheduler function prototypes ---------------------------------------- */

static void spencer_scheduler_init(void *impl);
static int  spencer_scheduler_schedule(void *impl, BruThread *thread);
static int  spencer_scheduler_schedule_in_order(void *impl, BruThread *thread);
static int  spencer_scheduler_has_next(const void *impl);
static BruThread *spencer_scheduler_next(void *impl);
static void       spencer_scheduler_free(void *impl);

BruScheduler *bru_spencer_scheduler_new(void)
{
    BruSpencerScheduler *ss = malloc(sizeof(*ss));
    BruScheduler        *s  = malloc(sizeof(*s));

    ss->in_order_idx = 0;
    ss->active       = NULL;
    stc_vec_default_init(ss->stack); // NOLINT(bugprone-sizeof-expression)

    s->impl              = ss;
    s->init              = spencer_scheduler_init;
    s->schedule          = spencer_scheduler_schedule;
    s->schedule_in_order = spencer_scheduler_schedule_in_order;
    s->has_next          = spencer_scheduler_has_next;
    s->next              = spencer_scheduler_next;
    s->free              = spencer_scheduler_free;

    return s;
}

/* --- SpencerScheduler function definitions -------------------------------- */

static void spencer_scheduler_init(void *impl)
{
    BruSpencerScheduler *self = impl;

    self->in_order_idx = 0;
    self->active       = NULL;
}

static int spencer_scheduler_schedule(void *impl, BruThread *thread)
{
    BruSpencerScheduler *self = impl;
    self->in_order_idx        = stc_vec_len_unsafe(self->stack) + 1;
    if (self->active)
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    else
        self->active = thread;
    return TRUE;
}

static int spencer_scheduler_schedule_in_order(void *impl, BruThread *thread)
{
    BruSpencerScheduler *self = impl;
    size_t               len  = stc_vec_len_unsafe(self->stack);

    if (self->in_order_idx > len) {
        spencer_scheduler_schedule(self, thread);
        self->in_order_idx = len;
    } else if (self->in_order_idx == len) {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_push_back(self->stack, thread);
    } else {
        // NOLINTNEXTLINE(bugprone-sizeof-expression)
        stc_vec_insert(self->stack, self->in_order_idx, thread);
    }

    return TRUE;
}

static int spencer_scheduler_has_next(const void *impl)
{
    const BruSpencerScheduler *self = impl;

    return self->active != NULL || !stc_vec_is_empty(self->stack);
}

static BruThread *spencer_scheduler_next(void *impl)
{
    BruSpencerScheduler *self   = impl;
    BruThread           *thread = self->active;

    self->in_order_idx = stc_vec_len_unsafe(self->stack) + 1;
    self->active       = NULL;
    if (thread == NULL && !stc_vec_is_empty(self->stack))
        thread = stc_vec_pop(self->stack);

    return thread;
}

static void spencer_scheduler_free(void *impl)
{
    BruSpencerScheduler *self = impl;
    stc_vec_free(self->stack);
    free(self);
}
