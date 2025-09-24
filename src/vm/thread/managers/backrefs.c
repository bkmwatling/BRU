#include <stdlib.h>
#include <string.h>

#include <bru/vm/thread/managers/backrefs.h>

/* --- Type definitions ----------------------------------------------------- */

typedef struct {
    bru_len_t backref_idx; /**< the next index into the captured string       */
} BruThreadWithBackref;

/* --- Function prototypes -------------------------------------------------- */

static void thread_init_with_backrefs(BruThreadManager *tm,
                                      BruThread        *thread,
                                      const bru_byte_t *pc,
                                      const char       *sp);
static void thread_copy_with_backrefs(BruThreadManager *tm,
                                      const BruThread  *src,
                                      BruThread        *dst);

static bru_len_t thread_get_backref_index(BruThreadManager *tm,
                                          const BruThread  *thread);

static void thread_set_backref_index(BruThreadManager *tm,
                                     BruThread        *thread,
                                     bru_len_t         val);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_tm_with_backrefs_new(BruThreadManager *tm)
{
    BruThreadManagerInterface *tmi, *super;

    // create thread manager instance
    super = bru_vt_curr(tm);
    tmi = bru_tmi_new(NULL, sizeof(BruThreadWithBackref) + super->_thread_size);

    // store functions
    tmi->init_thread       = thread_init_with_backrefs;
    tmi->copy_thread       = thread_copy_with_backrefs;
    tmi->get_backref_index = thread_get_backref_index;
    tmi->set_backref_index = thread_set_backref_index;

    // register extension
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- BruThreadManager function definitions -------------------------------- */

static void thread_init_with_backrefs(BruThreadManager *tm,
                                      BruThread        *thread,
                                      const bru_byte_t *pc,
                                      const char       *sp)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThreadWithBackref      *twb = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    twb->backref_idx = 0;

    bru_vt_call_super_procedure(tm, tmi, init_thread, thread, pc, sp);
}

static void thread_copy_with_backrefs(BruThreadManager *tm,
                                      const BruThread  *src,
                                      BruThread        *dst)
{
    BruThreadManagerInterface *tmi     = bru_vt_curr(tm);
    BruThreadWithBackref      *twb_src = BRU_THREAD_FROM_INSTANCE(tmi, src);
    BruThreadWithBackref      *twb_dst = BRU_THREAD_FROM_INSTANCE(tmi, dst);

    twb_dst->backref_idx = twb_src->backref_idx;

    bru_vt_call_super_procedure(tm, tmi, copy_thread, src, dst);
}

static bru_len_t thread_get_backref_index(BruThreadManager *tm,
                                          const BruThread  *thread)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThreadWithBackref      *twb = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    return twb->backref_idx;
}

static void
thread_set_backref_index(BruThreadManager *tm, BruThread *thread, bru_len_t val)
{
    BruThreadManagerInterface *tmi = bru_vt_curr(tm);
    BruThreadWithBackref      *twb = BRU_THREAD_FROM_INSTANCE(tmi, thread);

    twb->backref_idx = val;
}
