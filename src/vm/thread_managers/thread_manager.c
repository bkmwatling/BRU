#include "thread_manager.h"
#include "../../utils.h"

/* --- Thread manager NO-OP functions --------------------------------------- */

void bru_thread_manager_init_memoisation_noop(void       *thread_manager_impl,
                                              size_t      nmemo_insts,
                                              const char *text)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(nmemo_insts);
    BRU_UNUSED(text);
}

int bru_thread_manager_memoise_noop(void      *thread_manager_impl,
                                    BruThread *thread,
                                    bru_len_t  idx)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);

    return TRUE;
}

bru_cntr_t bru_thread_manager_counter_noop(void            *thread_manager_impl,
                                           const BruThread *thread,
                                           bru_len_t        idx)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    return 0;
}

void bru_thread_manager_set_counter_noop(void      *thread_manager_impl,
                                         BruThread *thread,
                                         bru_len_t  idx,
                                         bru_cntr_t val)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    BRU_UNUSED(val);
}

void bru_thread_manager_inc_counter_noop(void      *thread_manager_impl,
                                         BruThread *thread,
                                         bru_len_t  idx)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
}

void *bru_thread_manager_memory_noop(void            *thread_manager_impl,
                                     const BruThread *thread,
                                     bru_len_t        idx)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    return NULL;
}

void bru_thread_manager_set_memory_noop(void       *thread_manager_impl,
                                        BruThread  *thread,
                                        bru_len_t   idx,
                                        const void *val,
                                        size_t      size)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
    BRU_UNUSED(val);
    BRU_UNUSED(size);
}

const char *const *bru_thread_manager_captures_noop(void *thread_manager_impl,
                                                    const BruThread *thread,
                                                    bru_len_t       *ncaptures)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(ncaptures);
    return NULL;
}

void bru_thread_manager_set_capture_noop(void      *thread_manager_impl,
                                         BruThread *thread,
                                         bru_len_t  idx)
{
    BRU_UNUSED(thread_manager_impl);
    BRU_UNUSED(thread);
    BRU_UNUSED(idx);
}
