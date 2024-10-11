#include "write.h"
#include <string.h>

/* --- Data structures ------------------------------------------------------ */

typedef struct {
    bru_byte_t *tape; /**< stc_vec of bytes                                   */
} BruThreadWithWritableTape;

/* --- Function prototypes -------------------------------------------------- */

static void thread_init_with_write(BruThreadManager *tm,
                                   BruThread        *thread,
                                   const bru_byte_t *pc,
                                   const char       *sp);
static void thread_copy_with_write(BruThreadManager *tm,
                                   const BruThread  *src,
                                   BruThread        *dst);
static void thread_kill_with_write(BruThreadManager *tm, BruThread *thread);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_thread_manager_with_write_new(BruThreadManager *tm)
{
    BruThreadManagerInterface *tmi, *super;

    // create thread manager instance
    super = bru_vt_curr(tm);
    tmi   = bru_thread_manager_interface_new(
        NULL, sizeof(BruThreadWithWritableTape) + super->_thread_size);

    // store functions
    tmi->init_thread = thread_init_with_write;
    tmi->copy_thread = thread_copy_with_write;
    tmi->kill_thread = thread_kill_with_write;

    // register extension
    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruCapturesManager function definitions ------------------------------ */

static void thread_init_with_write(BruThreadManager *tm,
                                   BruThread        *thread,
                                   const bru_byte_t *pc,
                                   const char       *sp)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThreadWithWritableTape *twb =
        (BruThreadWithWritableTape *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_default_init(twb->tape);
    bru_vt_call_super_procedure(tm, tmi, init_thread, thread, pc, sp);
}

static void thread_copy_with_write(BruThreadManager *tm,
                                   const BruThread  *src,
                                   BruThread        *dst)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThreadWithWritableTape *twb_src =
        (BruThreadWithWritableTape *) BRU_THREAD_FROM_INSTANCE(tmi, src);
    BruThreadWithWritableTape *twb_dst =
        (BruThreadWithWritableTape *) BRU_THREAD_FROM_INSTANCE(tmi, dst);
    size_t i, len_src = stc_vec_len(twb_src->tape);

    stc_vec_clear(twb_dst);
    for (i = 0; i < len_src; i++)
        stc_vec_push_back(twb_dst->tape, twb_src->tape[i]);
    bru_vt_call_super_procedure(tm, tmi, copy_thread, src, dst);
}

static void thread_kill_with_write(BruThreadManager *tm, BruThread *thread)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThreadWithWritableTape *twb =
        (BruThreadWithWritableTape *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    // TODO: use thread_free interface function instead of thread_kill?
    stc_vec_free(twb->tape);
    bru_vt_call_super_procedure(tm, tmi, kill_thread, thread);
}

static void
thread_write_byte(BruThreadManager *tm, BruThread *thread, bru_byte_t byte)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThreadWithWritableTape *twb =
        (BruThreadWithWritableTape *) BRU_THREAD_FROM_INSTANCE(tmi, thread);

    stc_vec_push_back(twb->tape, byte);
}
