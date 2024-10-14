#include "thread_manager.h"
#include "../../utils.h"
#include <string.h>

/* --- Thread manager NO-OP functions --------------------------------------- */

BruThreadManagerInterface *bru_thread_manager_interface_new(void  *impl,
                                                            size_t tsize)
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
void bru_thread_manager_interface_free(BruThreadManagerInterface *tmi)
{
    free(tmi);
}

/* --- Thread manager NO-OP functions --------------------------------------- */

void bru_thread_manager_init_memoisation_noop(BruThreadManager *self,
                                              size_t            nmemo_insts,
                                              const char       *text)
{
    BRU_UNUSED(self);
    BRU_UNUSED(nmemo_insts);
    BRU_UNUSED(text);
}

int bru_thread_manager_memoise_noop(BruThreadManager *self,
                                    BruThread        *thread,
                                    bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);

    return TRUE;
}

bru_cntr_t bru_thread_manager_counter_noop(BruThreadManager *self,
                                           const BruThread  *thread,
                                           bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    return 0;
}

void bru_thread_manager_set_counter_noop(BruThreadManager *self,
                                         BruThread        *thread,
                                         bru_len_t         idx,
                                         bru_cntr_t        val)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    BRU_UNUSED(val);
}

void bru_thread_manager_inc_counter_noop(BruThreadManager *self,
                                         BruThread        *thread,
                                         bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
}

void *bru_thread_manager_memory_noop(BruThreadManager *self,
                                     const BruThread  *thread,
                                     bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    return NULL;
}

void bru_thread_manager_set_memory_noop(BruThreadManager *self,
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

void bru_thread_manager_write_byte_noop(BruThreadManager *self,

                                        BruThread *thread,
                                        bru_byte_t byte)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(byte);
}

bru_byte_t *bru_thread_manager_bytes_noop(BruThreadManager *self,
                                          BruThread        *thread)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    return NULL;
}

const char *const *bru_thread_manager_captures_noop(BruThreadManager *self,
                                                    const BruThread  *thread,
                                                    bru_len_t        *ncaptures)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(ncaptures);
    return NULL;
}

void bru_thread_manager_set_capture_noop(BruThreadManager *self,
                                         BruThread        *thread,
                                         bru_len_t         idx)
{
    BRU_UNUSED(self);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
}
