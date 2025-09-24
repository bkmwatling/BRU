#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <bru/vm/thread/managers/memoisation.h>

typedef struct {
    bool       *memoisation_memory; /**< memory used for memoisation          */
    const char *text;               /**< input string being matched against   */
    size_t      text_len;           /**< length of the input string           */
    size_t      nmemo_insts;        /**< the number of memo instructions      */
} BruMemoisedThreadManager;

/* --- MemoisedThreadManager function prototypes ---------------------------- */

static void memoised_tm_reset(BruThreadManager *tm);
static void memoised_tm_free(BruThreadManager *tm);

static void memoised_tm_init_memoisation(BruThreadManager *tm,
                                         size_t            nmemo_insts,
                                         const char       *text);
static bool
memoised_tm_memoise_check(BruThreadManager *tm, BruThread *t, bru_len_t idx);
static void
memoised_tm_memoise_set(BruThreadManager *tm, BruThread *t, bru_len_t idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_memoised_tm_new(BruThreadManager *tm)
{
    BruMemoisedThreadManager  *mtm = malloc(sizeof(*mtm));
    BruThreadManagerInterface *tmi, *super;

    mtm->memoisation_memory = NULL;
    mtm->text_len           = 0;
    mtm->nmemo_insts        = 0;

    super                 = bru_vt_curr(tm);
    tmi                   = bru_tmi_new(mtm, super->_thread_size);
    tmi->reset            = memoised_tm_reset;
    tmi->free             = memoised_tm_free;
    tmi->init_memoisation = memoised_tm_init_memoisation;
    tmi->memoise_check    = memoised_tm_memoise_check;
    tmi->memoise_set      = memoised_tm_memoise_set;

    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruMemoisedThreadManager function definitions ------------------------ */

static void memoised_tm_reset(BruThreadManager *tm)
{
    BruMemoisedThreadManager  *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);
    bru_vt_call_super_procedure(tm, tmi, reset);
    if (self->memoisation_memory)
        memset(self->memoisation_memory, false,
               self->nmemo_insts * self->text_len *
                   sizeof(*self->memoisation_memory));
}

static void memoised_tm_free(BruThreadManager *tm)
{
    BruMemoisedThreadManager  *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);

    free(self->memoisation_memory);
    free(self);

    bru_vt_call_super_procedure(tm, tmi, free);
}

static void memoised_tm_init_memoisation(BruThreadManager *tm,
                                         size_t            nmemo_insts,
                                         const char       *text)
{
    BruMemoisedThreadManager *self = bru_vt_curr_impl(tm);

    if (self->memoisation_memory) free(self->memoisation_memory);

    self->text               = text;
    self->text_len           = strlen(text) + 1;
    self->nmemo_insts        = nmemo_insts;
    self->memoisation_memory = malloc(nmemo_insts * self->text_len *
                                      sizeof(*self->memoisation_memory));
    memset(self->memoisation_memory, false,
           nmemo_insts * self->text_len * sizeof(*self->memoisation_memory));
}

static bool
memoised_tm_memoise_check(BruThreadManager *tm, BruThread *t, bru_len_t idx)
{
    BruMemoisedThreadManager *self = bru_vt_curr_impl(tm);

    size_t i = idx * self->text_len + (bru_tm_sp(tm, t) - self->text);

    return self->memoisation_memory[i];
}

static void
memoised_tm_memoise_set(BruThreadManager *tm, BruThread *t, bru_len_t idx)
{
    BruMemoisedThreadManager *self = bru_vt_curr_impl(tm);

    size_t i = idx * self->text_len + (bru_tm_sp(tm, t) - self->text);

    self->memoisation_memory[i] = true;
}
