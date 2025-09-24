#include <stdlib.h>
#include <string.h>

#include <bru/vm/thread/managers/write.h>

/* --- Preprocessor directives ---------------------------------------------- */

#define WRITABLE_THREAD_SIZE \
    (sizeof(struct stc_vec_header) + sizeof(bru_byte_t *))
#define WRITABLE_THREAD_FROM_INSTANCE(instance, thread)                       \
    ((void *) (((struct stc_vec_header *) BRU_THREAD_FROM_INSTANCE(instance,  \
                                                                   thread)) + \
               1))

/* --- Function prototypes -------------------------------------------------- */

static BruThread *thread_alloc_with_write(BruThreadManager *tm);
static void       thread_copy_with_write(BruThreadManager *tm,
                                         const BruThread  *src,
                                         BruThread        *dst);
static void thread_free_with_write(BruThreadManager *tm, BruThread *thread);
static bru_byte_t *
thread_read_bytes(BruThreadManager *tm, BruThread *thread, size_t *nbytes);
static void
thread_write_byte(BruThreadManager *tm, BruThread *thread, bru_byte_t byte);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_tm_with_write_new(BruThreadManager *tm)
{
    BruThreadManagerInterface *tmi, *super;

    // create thread manager instance
    super = bru_vt_curr(tm);
    tmi   = bru_tmi_new(NULL, WRITABLE_THREAD_SIZE + super->_thread_size);

    // store functions
    tmi->alloc_thread = thread_alloc_with_write;
    tmi->copy_thread  = thread_copy_with_write;
    tmi->free_thread  = thread_free_with_write;
    tmi->read_bytes   = thread_read_bytes;
    tmi->write_byte   = thread_write_byte;

    // register extension
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruCapturesManager function definitions ------------------------------ */

static BruThread *thread_alloc_with_write(BruThreadManager *tm)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThread *thread       = bru_vt_call_super_function(tm, tmi, alloc_thread);
    StcVec(bru_byte_t) *twb = WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_default_init(twb);

    return thread;
}

static void thread_copy_with_write(BruThreadManager *tm,
                                   const BruThread  *src,
                                   BruThread        *dst)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    StcVec(bru_byte_t) *twb_src    = WRITABLE_THREAD_FROM_INSTANCE(tmi, src);
    StcVec(bru_byte_t) *twb_dst    = WRITABLE_THREAD_FROM_INSTANCE(tmi, dst);
    size_t              i, len_src = stc_vec_len(*twb_src);

    stc_vec_clear(twb_dst);
    for (i = 0; i < len_src; i++) stc_vec_push_back(twb_dst, (*twb_src)[i]);
    bru_vt_call_super_procedure(tm, tmi, copy_thread, src, dst);
}

static void thread_free_with_write(BruThreadManager *tm, BruThread *thread)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    StcVec(bru_byte_t)        *twb = WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_free(*twb);
    bru_vt_call_super_procedure(tm, tmi, free_thread, thread);
}

static void
thread_write_byte(BruThreadManager *tm, BruThread *thread, bru_byte_t byte)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    StcVec(bru_byte_t)        *twb = WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_push_back(twb, byte);
}

static bru_byte_t *
thread_read_bytes(BruThreadManager *tm, BruThread *thread, size_t *nbytes)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    StcVec(bru_byte_t)        *twb = WRITABLE_THREAD_FROM_INSTANCE(tmi, thread);
    size_t                     bytes_len = stc_vec_len(*twb);
    bru_byte_t                *bytes     = malloc(sizeof(*bytes) * bytes_len);

    memcpy(bytes, *twb, sizeof(*bytes) * bytes_len);
    if (nbytes) *nbytes = bytes_len;

    return bytes;
}
