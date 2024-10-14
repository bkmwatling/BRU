#include <stdlib.h>
#include <string.h>

#include "memoisation.h"

typedef struct {
    bru_byte_t *memoisation_memory; /**< memory used for memoisation          */
    const char *text;               /**< input string being matched against   */
    size_t      text_len;           /**< length of the input string           */
    size_t      nmemo_insts;        /**< the number of memo instructions      */
} BruMemoisedThreadManager;

/* --- MemoisedThreadManager function prototypes ---------------------------- */

static void memoised_thread_manager_reset(BruThreadManager *tm);
static void memoised_thread_manager_free(BruThreadManager *tm);

static void memoised_thread_manager_init_memoisation(BruThreadManager *tm,
                                                     size_t      nmemo_insts,
                                                     const char *text);
static int  memoised_thread_manager_memoise(BruThreadManager *tm,
                                            BruThread        *t,
                                            bru_len_t         idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_memoised_thread_manager_new(BruThreadManager *tm)
{
    BruMemoisedThreadManager  *mtm = malloc(sizeof(*mtm));
    BruThreadManagerInterface *tmi, *super;

    mtm->memoisation_memory = NULL;
    mtm->text_len           = 0;
    mtm->nmemo_insts        = 0;

    super      = bru_vt_curr(tm);
    tmi        = bru_thread_manager_interface_new(mtm, super->_thread_size);
    tmi->reset = memoised_thread_manager_reset;
    tmi->free  = memoised_thread_manager_free;
    tmi->init_memoisation = memoised_thread_manager_init_memoisation;
    tmi->memoise          = memoised_thread_manager_memoise;

    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruMemoisedThreadManager function definitions ------------------------ */

static void memoised_thread_manager_reset(BruThreadManager *tm)
{
    BruMemoisedThreadManager  *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);
    bru_vt_call_super_procedure(tm, tmi, reset);
    if (self->memoisation_memory)
        memset(self->memoisation_memory, FALSE,
               self->nmemo_insts * self->text_len *
                   sizeof(*self->memoisation_memory));
}

static void memoised_thread_manager_free(BruThreadManager *tm)
{
    BruMemoisedThreadManager  *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface *tmi  = bru_vt_curr(tm);

    free(self->memoisation_memory);
    free(self);

    bru_vt_call_super_procedure(tm, tmi, free);
}

static void memoised_thread_manager_init_memoisation(BruThreadManager *tm,
                                                     size_t      nmemo_insts,
                                                     const char *text)
{
    BruMemoisedThreadManager *self = bru_vt_curr_impl(tm);

    if (self->memoisation_memory) free(self->memoisation_memory);

    self->text               = text;
    self->text_len           = strlen(text) + 1;
    self->nmemo_insts        = nmemo_insts;
    self->memoisation_memory = malloc(nmemo_insts * self->text_len *
                                      sizeof(*self->memoisation_memory));
    memset(self->memoisation_memory, FALSE,
           nmemo_insts * self->text_len * sizeof(*self->memoisation_memory));
}

static int memoised_thread_manager_memoise(BruThreadManager *tm,
                                           BruThread        *t,
                                           bru_len_t         idx)
{
    BruMemoisedThreadManager *self = bru_vt_curr_impl(tm);
    const char               *_sp;

    size_t i =
        idx * self->text_len + (bru_thread_manager_sp(tm, _sp, t) - self->text);

    if (self->memoisation_memory[i]) return FALSE;
    self->memoisation_memory[i] = TRUE;

    return TRUE;
}
