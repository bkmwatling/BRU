#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <bru/utils.h>
#include <bru/vm/thread/managers/manager.h>

/* --- Thread manager interface functions ----------------------------------- */

BruThreadManagerInterface *bru_tmi_new(void *impl, size_t tsize)
{
    BruThreadManagerInterface *tmi = malloc(sizeof(*tmi));

    memset(tmi, 0, sizeof(*tmi));
    tmi->__vt_impl    = impl;
    tmi->_thread_size = tsize;

    return tmi;
}

/**
 * Free the thread manager interface.
 *
 * @param[in] tmi the thread manager interface
 */
void bru_tmi_free(BruThreadManagerInterface *tmi) { free(tmi); }

/* --- Thread manager NO-OP functions --------------------------------------- */

void bru_tm_init_memoisation_noop(BruThreadManager *self,
                                  size_t            nmemo_insts,
                                  const char       *text)
{
    BRU_UNUSED(self);
    BRU_UNUSED(nmemo_insts);
    BRU_UNUSED(text);
}

bool bru_tm_memoise_check_noop(BruThreadManager *self,
                               BruThread        *thread,
                               bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);

    return true;
}

void bru_tm_memoise_set_noop(BruThreadManager *self,
                             BruThread        *thread,
                             bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
}

bru_cntr_t bru_tm_get_counter_noop(BruThreadManager *self,
                                   const BruThread  *thread,
                                   bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    return 0;
}

void bru_tm_set_counter_noop(BruThreadManager *self,
                             BruThread        *thread,
                             bru_len_t         idx,
                             bru_cntr_t        val)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    BRU_UNUSED(val);
}

void bru_tm_inc_counter_noop(BruThreadManager *self,
                             BruThread        *thread,
                             bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
}

void *bru_tm_get_memory_noop(BruThreadManager *self,
                             const BruThread  *thread,
                             bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    return NULL;
}

void bru_tm_set_memory_noop(BruThreadManager *self,
                            BruThread        *thread,
                            bru_len_t         idx,
                            const void       *val,
                            size_t            size)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    BRU_UNUSED(val);
    BRU_UNUSED(size);
}

void bru_tm_write_byte_noop(BruThreadManager *self,
                            BruThread        *thread,
                            bru_byte_t        byte)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(byte);
}

bru_byte_t *bru_tm_read_bytes_noop(BruThreadManager *self,
                                   BruThread        *thread,
                                   size_t           *nbytes)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(nbytes);
    return NULL;
}

const char *const *bru_tm_get_captures_noop(BruThreadManager *self,
                                            const BruThread  *thread,
                                            bru_len_t        *ncaptures)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(ncaptures);
    return NULL;
}

void bru_tm_set_capture_noop(BruThreadManager *self,
                             BruThread        *thread,
                             bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
}

const char *bru_tm_get_capture_noop(BruThreadManager *self,
                                    const BruThread  *thread,
                                    bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);

    return NULL;
}

bru_len_t bru_tm_get_backref_index_noop(BruThreadManager *self,
                                        const BruThread  *thread)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    return 0;
}

void bru_tm_set_backref_index_noop(BruThreadManager *self,
                                   BruThread        *thread,
                                   bru_len_t         val)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(val);
}
