#include <stdlib.h>
#include <string.h>

#include "all_matches.h"

typedef struct {
    FILE       *logfile; /**< the file stream for logging captures on match   */
    const char *text;    /**< the input string being matched against          */

    BruThreadManager *__manager; /**< the thread manager being wrapped        */
} AllMatchesThreadManager;

/* --- AllMatchesThreadManager function prototypes -------------------------- */

static void all_matches_thread_manager_init(void             *impl,
                                            const bru_byte_t *start_pc,
                                            const char       *start_sp);
static void all_matches_thread_manager_reset(void *impl);
static void all_matches_thread_manager_free(void *impl);
static int  all_matches_thread_manager_done_exec(void *impl);

static void all_matches_thread_manager_schedule_thread(void      *impl,
                                                       BruThread *t);
static void all_matches_thread_manager_schedule_thread_in_order(void      *impl,
                                                                BruThread *t);
static BruThread *all_matches_thread_manager_next_thread(void *impl);
static void       all_matches_thread_manager_notify_thread_match(void      *impl,
                                                                 BruThread *t);
static BruThread *all_matches_thread_manager_clone_thread(void            *impl,
                                                          const BruThread *t);
static void all_matches_thread_manager_kill_thread(void *impl, BruThread *t);
static void all_matches_thread_manager_init_memoisation(void       *impl,
                                                        size_t      nmemo_insts,
                                                        const char *text);

static const bru_byte_t *all_matches_thread_pc(void *impl, const BruThread *t);
static void
all_matches_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc);
static const char *all_matches_thread_sp(void *impl, const BruThread *t);
static void        all_matches_thread_inc_sp(void *impl, BruThread *t);
static int all_matches_thread_memoise(void *impl, BruThread *t, bru_len_t idx);
static bru_cntr_t
all_matches_thread_counter(void *impl, const BruThread *t, bru_len_t idx);
static void all_matches_thread_set_counter(void      *impl,
                                           BruThread *t,
                                           bru_len_t  idx,
                                           bru_cntr_t val);
static void
all_matches_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx);
static void *
all_matches_thread_memory(void *impl, const BruThread *t, bru_len_t idx);
static void               all_matches_thread_set_memory(void       *impl,
                                                        BruThread  *t,
                                                        bru_len_t   idx,
                                                        const void *val,
                                                        size_t      size);
static const char *const *all_matches_thread_captures(void            *impl,
                                                      const BruThread *t,
                                                      bru_len_t *ncaptures);
static void
all_matches_thread_set_capture(void *impl, BruThread *t, bru_len_t idx);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *
bru_all_matches_thread_manager_new(BruThreadManager *thread_manager,
                                   FILE             *logfile,
                                   const char       *text)
{
    AllMatchesThreadManager *amtm = malloc(sizeof(*amtm));
    BruThreadManager        *tm   = malloc(sizeof(*tm));

    amtm->logfile   = logfile;
    amtm->text      = text;
    amtm->__manager = thread_manager;

    BRU_THREAD_MANAGER_SET_ALL_FUNCS(tm, all_matches);
    tm->impl = amtm;

    return tm;
}

/* --- AllMatchesThreadManager function definitions ------------------------- */

static void all_matches_thread_manager_init(void             *impl,
                                            const bru_byte_t *start_pc,
                                            const char       *start_sp)
{
    bru_thread_manager_init(((AllMatchesThreadManager *) impl)->__manager,
                            start_pc, start_sp);
}

static void print_match(AllMatchesThreadManager *self, BruThread *t)
{
    const char  *capture;
    const char **captures;
    bru_len_t    i, ncaptures;
    int          ncodepoints;

    if (!t) return;

    fprintf(self->logfile, "matched = TRUE\n");
    fprintf(self->logfile, "captures:\n");
    bru_thread_manager_captures(self->__manager, t, &ncaptures);
    captures = malloc(2 * ncaptures * sizeof(const char *));
    memcpy(captures,
           bru_thread_manager_captures(self->__manager, t, &ncaptures),
           2 * ncaptures * sizeof(char *));
    fprintf(self->logfile, "  input: '%s'\n", self->text);
    for (i = 0; i < 2 * ncaptures; i += 2) {
        capture = captures[i];
        fprintf(self->logfile, "%7hu: ", i);
        if (capture) {
            ncodepoints = stc_utf8_str_ncodepoints(self->text) -
                          stc_utf8_str_ncodepoints(capture);
            fprintf(self->logfile, "%.*s'%.*s'\n", ncodepoints, "",
                    (int) (captures[i + 1] - capture), capture);
        } else {
            fprintf(self->logfile, "not captured\n");
        }
    }
    free(captures);
}

static void all_matches_thread_manager_reset(void *impl)
{
    AllMatchesThreadManager *self = impl;
    bru_thread_manager_reset(self->__manager);
}

static void all_matches_thread_manager_free(void *impl)
{
    AllMatchesThreadManager *self = impl;
    bru_thread_manager_free(self->__manager);
    free(impl);
}

static int all_matches_thread_manager_done_exec(void *impl)
{
    return bru_thread_manager_done_exec(
        ((AllMatchesThreadManager *) impl)->__manager);
}

static void all_matches_thread_manager_schedule_thread(void *impl, BruThread *t)
{
    bru_thread_manager_schedule_thread(
        ((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_manager_schedule_thread_in_order(void      *impl,
                                                                BruThread *t)
{
    bru_thread_manager_schedule_thread_in_order(
        ((AllMatchesThreadManager *) impl)->__manager, t);
}

static BruThread *all_matches_thread_manager_next_thread(void *impl)
{
    AllMatchesThreadManager *self = impl;
    BruThread *t = bru_thread_manager_next_thread(self->__manager);

    return t;
}

static void all_matches_thread_manager_notify_thread_match(void      *impl,
                                                           BruThread *t)
{
    AllMatchesThreadManager *self = impl;

    print_match(self, t);
    bru_thread_manager_kill_thread(self->__manager, t);

    // thread_manager_notify_thread_match(self->__manager, t);
}

static BruThread *all_matches_thread_manager_clone_thread(void            *impl,
                                                          const BruThread *t)
{
    return bru_thread_manager_clone_thread(
        ((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_manager_kill_thread(void *impl, BruThread *t)
{
    AllMatchesThreadManager *self = impl;
    bru_thread_manager_kill_thread(self->__manager, t);
}

static const bru_byte_t *all_matches_thread_pc(void *impl, const BruThread *t)
{
    return bru_thread_manager_pc(((AllMatchesThreadManager *) impl)->__manager,
                                 t);
}

static void
all_matches_thread_set_pc(void *impl, BruThread *t, const bru_byte_t *pc)
{
    bru_thread_manager_set_pc(((AllMatchesThreadManager *) impl)->__manager, t,
                              pc);
}

static const char *all_matches_thread_sp(void *impl, const BruThread *t)
{
    return bru_thread_manager_sp(((AllMatchesThreadManager *) impl)->__manager,
                                 t);
}

static void all_matches_thread_inc_sp(void *impl, BruThread *t)
{
    bru_thread_manager_inc_sp(((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_manager_init_memoisation(void       *impl,
                                                        size_t      nmemo_insts,
                                                        const char *text)
{
    bru_thread_manager_init_memoisation(
        ((AllMatchesThreadManager *) impl)->__manager, nmemo_insts, text);
}

static int all_matches_thread_memoise(void *impl, BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_memoise(
        ((AllMatchesThreadManager *) impl)->__manager, t, idx);
}

static bru_cntr_t
all_matches_thread_counter(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_counter(
        ((AllMatchesThreadManager *) impl)->__manager, t, idx);
}

static void all_matches_thread_set_counter(void      *impl,
                                           BruThread *t,
                                           bru_len_t  idx,
                                           bru_cntr_t val)
{
    bru_thread_manager_set_counter(
        ((AllMatchesThreadManager *) impl)->__manager, t, idx, val);
}

static void
all_matches_thread_inc_counter(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_inc_counter(
        ((AllMatchesThreadManager *) impl)->__manager, t, idx);
}

static void *
all_matches_thread_memory(void *impl, const BruThread *t, bru_len_t idx)
{
    return bru_thread_manager_memory(
        ((AllMatchesThreadManager *) impl)->__manager, t, idx);
}

static void all_matches_thread_set_memory(void       *impl,
                                          BruThread  *t,
                                          bru_len_t   idx,
                                          const void *val,
                                          size_t      size)
{
    bru_thread_manager_set_memory(((AllMatchesThreadManager *) impl)->__manager,
                                  t, idx, val, size);
}

static const char *const *all_matches_thread_captures(void            *impl,
                                                      const BruThread *t,
                                                      bru_len_t *ncaptures)
{
    return bru_thread_manager_captures(
        ((AllMatchesThreadManager *) impl)->__manager, t, ncaptures);
}

static void
all_matches_thread_set_capture(void *impl, BruThread *t, bru_len_t idx)
{
    bru_thread_manager_set_capture(
        ((AllMatchesThreadManager *) impl)->__manager, t, idx);
}
