#include <stdlib.h>
#include <string.h>

#include <bru/vm/thread/managers/captures.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    bru_len_t ncaptures; /**< number of captures to allocate space for        */
} BruThreadManagerWithCaptures;

/* --- Function prototypes -------------------------------------------------- */

static void tm_with_captures_free(BruThreadManager *tm);

static void thread_init_with_captures(BruThreadManager *tm,
                                      BruThread        *thread,
                                      const bru_byte_t *pc,
                                      const char       *sp);
static void thread_copy_with_captures(BruThreadManager *tm,
                                      const BruThread  *src,
                                      BruThread        *dst);

static const char *const *thread_get_captures(BruThreadManager *tm,
                                              const BruThread  *thread,
                                              bru_len_t        *ncaptures);
static void
thread_set_capture(BruThreadManager *tm, BruThread *thread, bru_len_t idx);

static const char *thread_get_capture(BruThreadManager *tm,
                                      const BruThread  *thread,
                                      bru_len_t         idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_tm_with_captures_new(BruThreadManager *tm,
                                           bru_len_t         ncaptures)
{
    BruThreadManagerInterface    *tmi, *super;
    BruThreadManagerWithCaptures *impl = malloc(sizeof(*impl));

    // populate impl
    impl->ncaptures = ncaptures;

    // create thread manager instance
    super = bru_vt_curr(tm);
    tmi   = bru_tm_interface_new(impl, 2 * ncaptures * sizeof(const char *) +
                                           super->_thread_size);

    // store functions
    tmi->free        = tm_with_captures_free;
    tmi->init_thread = thread_init_with_captures;
    tmi->copy_thread = thread_copy_with_captures;
    tmi->captures    = thread_get_captures;
    tmi->set_capture = thread_set_capture;
    tmi->get_capture = thread_get_capture;

    // register extension
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruCapturesManager function definitions ------------------------------ */

static void tm_with_captures_free(BruThreadManager *tm)
{
    BruThreadManagerWithCaptures *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);

    free(self);

    bru_vt_call_super_procedure(tm, tmi, free);
}

static void thread_init_with_captures(BruThreadManager *tm,
                                      BruThread        *thread,
                                      const bru_byte_t *pc,
                                      const char       *sp)
{
    BruThreadManagerWithCaptures *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);
    const char **captures              = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    memset(captures, 0, sizeof(*captures) * 2 * self->ncaptures);
    bru_vt_call_super_procedure(tm, tmi, init_thread, thread, pc, sp);
}

static void thread_copy_with_captures(BruThreadManager *tm,
                                      const BruThread  *src,
                                      BruThread        *dst)
{
    BruThreadManagerWithCaptures *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);
    const char **src_captures          = BRU_THREAD_FROM_INSTANCE(tmi, src);
    const char **dst_captures          = BRU_THREAD_FROM_INSTANCE(tmi, dst);

    memcpy(dst_captures, src_captures,
           sizeof(*src_captures) * 2 * self->ncaptures);

    bru_vt_call_super_procedure(tm, tmi, copy_thread, src, dst);
}

static const char *const *thread_get_captures(BruThreadManager *tm,
                                              const BruThread  *thread,
                                              bru_len_t        *ncaptures)
{
    BruThreadManagerWithCaptures *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface    *tmi  = bru_vt_curr(tm);
    const char **captures              = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    if (ncaptures) *ncaptures = self->ncaptures;
    return captures;
}

static void
thread_set_capture(BruThreadManager *tm, BruThread *thread, bru_len_t idx)
{
    BruThreadManagerInterface *tmi      = bru_vt_curr(tm);
    const char               **captures = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    captures[idx] = bru_tm_sp(tm, thread);
}

static const char *
thread_get_capture(BruThreadManager *tm, const BruThread *thread, bru_len_t idx)
{
    BruThreadManagerInterface *tmi      = bru_vt_curr(tm);
    const char               **captures = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    return captures[idx];
}
