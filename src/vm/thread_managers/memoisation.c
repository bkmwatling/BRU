#include <stdlib.h>
#include <string.h>

#include "memoisation.h"

typedef struct {
    bru_byte_t *memoisation_memory; /**< memory used for memoisation          */
    const char *text;               /**< input string being matched against   */
    size_t      text_len;           /**< length of the input string           */

    BruThreadManager *__manager; /**< the thread manager being wrapped        */
} BruMemoisedThreadManager;

/* --- MemoisedThreadManager function prototypes ---------------------------- */

static void memoised_thread_manager_init(void             *impl,
                                         const bru_byte_t *start_pc,
                                         const char       *start_sp);
static void memoised_thread_manager_reset(void *impl);
static void memoised_thread_manager_free(void *impl);
static int  memoised_thread_manager_done_exec(void *impl);

static void memoised_thread_manager_schedule_thread(void *impl, BruThread *t);
static void memoised_thread_manager_schedule_thread_in_order(void      *impl,
                                                             BruThread *t);
static BruThread *memoised_thread_manager_next_thread(void *impl);
static void       memoised_thread_manager_notify_thread_match(void      *impl,
                                                              BruThread *t);
static BruThread *memoised_thread_manager_clone_thread(void            *impl,
                                                       const BruThread *t);
static void       memoised_thread_manager_kill_thread(void *impl, BruThread *t);
static void       memoised_thread_manager_init_memoisation(void       *impl,
                                                           size_t      nmemo_insts,
                                                           const char *text);

static const bru_byte_t *memoised_thread_pc(void *impl, const BruThread *t);
static void
memoised_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc);
static const char *memoised_thread_sp(void *impl, const BruThread *t);
static void        memoised_thread_inc_sp(void *impl, BruThread *t);
static int memoised_thread_memoise(void *impl, BruThread *t, bru_len_t idx);
static bru_cntr_t
memoised_thread_counter(void *impl, const BruThread *t, bru_len_t idx);
static void memoised_thread_set_counter(void      *impl,
                                        BruThread *t,
                                        bru_len_t  idx,
                                        bru_cntr_t val);
static void
memoised_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx);
static void *
memoised_thread_memory(void *impl, const BruThread *t, bru_len_t idx);
static void memoised_thread_set_memory(void       *impl,
                                       BruThread  *t,
                                       bru_len_t   idx,
                                       const void *val,
                                       size_t      size);
static const char *const *
memoised_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures);
static void
memoised_thread_set_capture(void *impl, BruThread *t, bru_len_t idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *
bru_memoised_thread_manager_new(BruThreadManager *thread_manager)
{
    BruMemoisedThreadManager *mtm = malloc(sizeof(*mtm));
    BruThreadManager         *tm  = malloc(sizeof(*tm));

    mtm->memoisation_memory = NULL;
    mtm->text_len           = 0;
    mtm->text_len           = 0;
    mtm->__manager          = thread_manager;

    BRU_THREAD_MANAGER_SET_ALL_FUNCS(tm, memoised);
    tm->impl = mtm;

    return tm;
}

/* --- BruMemoisedThreadManager function definitions ------------------------ */

static void memoised_thread_manager_init(void             *impl,
                                         const bru_byte_t *start_pc,
                                         const char       *start_sp)
{
    bru_thread_manager_init(((BruMemoisedThreadManager *) impl)->__manager,
                            start_pc, start_sp);
}

static void memoised_thread_manager_reset(void *impl)
{
    BruMemoisedThreadManager *self = impl;
    bru_thread_manager_reset(self->__manager);
    if (self->memoisation_memory)
        memset(self->memoisation_memory, 0,
               self->text_len * self->text_len *
                   sizeof(*self->memoisation_memory));
}

static void memoised_thread_manager_free(void *impl)
{
    free(((BruMemoisedThreadManager *) impl)->memoisation_memory);
    bru_thread_manager_free(((BruMemoisedThreadManager *) impl)->__manager);
    free(impl);
}

static int memoised_thread_manager_done_exec(void *impl)
{
    return bru_thread_manager_done_exec(
        ((BruMemoisedThreadManager *) impl)->__manager);
}

static void memoised_thread_manager_schedule_thread(void *impl, BruThread *t)
{
    bru_thread_manager_schedule_thread(
        ((BruMemoisedThreadManager *) impl)->__manager, t);
}

static void memoised_thread_manager_schedule_thread_in_order(void      *impl,
                                                             BruThread *t)
{
    bru_thread_manager_schedule_thread_in_order(
        ((BruMemoisedThreadManager *) impl)->__manager, t);
}

static BruThread *memoised_thread_manager_next_thread(void *impl)
{
    return bru_thread_manager_next_thread(
        ((BruMemoisedThreadManager *) impl)->__manager);
}

static void memoised_thread_manager_notify_thread_match(void      *impl,
                                                        BruThread *t)
{
    bru_thread_manager_notify_thread_match(
        ((BruMemoisedThreadManager *) impl)->__manager, t);
}

static BruThread *memoised_thread_manager_clone_thread(void            *impl,
                                                       const BruThread *t)
{
    return bru_thread_manager_clone_thread(
        ((BruMemoisedThreadManager *) impl)->__manager, t);
}

static void memoised_thread_manager_kill_thread(void *impl, BruThread *t)
{
    bru_thread_manager_kill_thread(
        ((BruMemoisedThreadManager *) impl)->__manager, t);
}

static const bru_byte_t *memoised_thread_pc(void *impl, const BruThread *t)
{
    return bru_thread_manager_pc(((BruMemoisedThreadManager *) impl)->__manager,
                                 t);
}

static void
memoised_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc)
{
    bru_thread_manager_set_pc(((BruMemoisedThreadManager *) impl)->__manager, t,
                              pc);
}

static const char *memoised_thread_sp(void *impl, const BruThread *t)
{
    return bru_thread_manager_sp(((BruMemoisedThreadManager *) impl)->__manager,
                                 t);
}

static void memoised_thread_inc_sp(void *impl, BruThread *t)
{
    bru_thread_manager_inc_sp(((BruMemoisedThreadManager *) impl)->__manager,
                              t);
}

static void memoised_thread_manager_init_memoisation(void       *impl,
                                                     size_t      nmemo_insts,
                                                     const char *text)
{
    BruMemoisedThreadManager *self = impl;

    if (self->memoisation_memory) free(self->memoisation_memory);

    self->text               = text;
    self->text_len           = strlen(text) + 1;
    self->memoisation_memory = malloc(nmemo_insts * self->text_len *
                                      sizeof(*self->memoisation_memory));
    memset(self->memoisation_memory, FALSE,
           nmemo_insts * self->text_len * sizeof(*self->memoisation_memory));
}

static int memoised_thread_memoise(void *impl, BruThread *t, bru_len_t idx)
{
    BruMemoisedThreadManager *self = impl;
    size_t                    i = idx * self->text_len + (t->sp - self->text);

    if (self->memoisation_memory[i]) return FALSE;
    self->memoisation_memory[i] = TRUE;

    return TRUE;
}

static bru_cntr_t
memoised_thread_counter(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_counter(
        ((BruMemoisedThreadManager *) impl)->__manager, t, idx);
}

static void memoised_thread_set_counter(void      *impl,
                                        BruThread *t,
                                        bru_len_t  idx,
                                        bru_cntr_t val)
{
    bru_thread_manager_set_counter(
        ((BruMemoisedThreadManager *) impl)->__manager, t, idx, val);
}

static void memoised_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_inc_counter(
        ((BruMemoisedThreadManager *) impl)->__manager, t, idx);
}

static void *
memoised_thread_memory(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_memory(
        ((BruMemoisedThreadManager *) impl)->__manager, t, idx);
}

static void memoised_thread_set_memory(void       *impl,
                                       BruThread  *t,
                                       bru_len_t   idx,
                                       const void *val,
                                       size_t      size)
{
    bru_thread_manager_set_memory(
        ((BruMemoisedThreadManager *) impl)->__manager, t, idx, val, size);
}

static const char *const *
memoised_thread_captures(void *impl, const BruThread *t, bru_len_t *ncaptures)
{
    return bru_thread_manager_captures(
        ((BruMemoisedThreadManager *) impl)->__manager, t, ncaptures);
}

static void memoised_thread_set_capture(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_set_capture(
        ((BruMemoisedThreadManager *) impl)->__manager, t, idx);
}
