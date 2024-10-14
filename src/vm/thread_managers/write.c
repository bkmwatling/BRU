#include "write.h"
#include <string.h>

/* --- Preprocessor directives ---------------------------------------------- */

#define WRITABLE_THREAD_SIZE (sizeof(StcVecHeader) + sizeof(bru_byte_t *))
#define WRITABLE_THREAD_FROM_INSTANCE(instance, thread) \
    (BRU_THREAD_FROM_INSTANCE(instance, thread) + sizeof(StcVecHeader))

/* --- Function prototypes -------------------------------------------------- */

static BruThread *thread_alloc_with_write(BruThreadManager *tm);
static void       thread_copy_with_write(BruThreadManager *tm,
                                         const BruThread  *src,
                                         BruThread        *dst);
static void thread_free_with_write(BruThreadManager *tm, BruThread *thread);
static bru_byte_t *
thread_bytes(BruThreadManager *tm, BruThread *thread, size_t *nbytes);
static void
thread_write_byte(BruThreadManager *tm, BruThread *thread, bru_byte_t byte);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_thread_manager_with_write_new(BruThreadManager *tm)
{
    BruThreadManagerInterface *tmi, *super;

    // create thread manager instance
    super = bru_vt_curr(tm);
    tmi   = bru_thread_manager_interface_new(NULL, WRITABLE_THREAD_SIZE +
                                                       super->_thread_size);

    // store functions
    tmi->alloc_thread = thread_alloc_with_write;
    tmi->copy_thread  = thread_copy_with_write;
    tmi->free_thread  = thread_free_with_write;
    tmi->bytes        = thread_bytes;
    tmi->write_byte   = thread_write_byte;

    // register extension
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruCapturesManager function definitions ------------------------------ */

static BruThread *thread_alloc_with_write(BruThreadManager *tm)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThread                 *thread =
        bru_vt_call_super_function(tm, tmi, thread, alloc_thread);
    bru_byte_t **twb =
        (bru_byte_t **) WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_default_init(*twb);

    return thread;
}

static void thread_copy_with_write(BruThreadManager *tm,
                                   const BruThread  *src,
                                   BruThread        *dst)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_byte_t               **twb_src =
        (bru_byte_t **) WRITABLE_THREAD_FROM_INSTANCE(tmi, src);
    bru_byte_t **twb_dst =
        (bru_byte_t **) WRITABLE_THREAD_FROM_INSTANCE(tmi, dst);
    size_t i, len_src = stc_vec_len(*twb_src);

    stc_vec_clear(*twb_dst);
    for (i = 0; i < len_src; i++) stc_vec_push_back(*twb_dst, (*twb_src)[i]);
    bru_vt_call_super_procedure(tm, tmi, copy_thread, src, dst);
}

static void thread_free_with_write(BruThreadManager *tm, BruThread *thread)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_byte_t               **twb =
        (bru_byte_t **) WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_free(*twb);
    bru_vt_call_super_procedure(tm, tmi, free_thread, thread);
}

static void
thread_write_byte(BruThreadManager *tm, BruThread *thread, bru_byte_t byte)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_byte_t               **twb =
        (bru_byte_t **) WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_push_back(*twb, byte);
}

static bru_byte_t *
thread_bytes(BruThreadManager *tm, BruThread *thread, size_t *nbytes)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    bru_byte_t               **twb =
        (bru_byte_t **) WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);
    size_t      bytes_len = stc_vec_len(*twb);
    bru_byte_t *bytes     = malloc(sizeof(*bytes) * bytes_len);

    memcpy(bytes, *twb, sizeof(*bytes) * bytes_len);
    if (nbytes) *nbytes = bytes_len;

    return bytes;
}
