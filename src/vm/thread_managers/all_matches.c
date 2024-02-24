#include <stdlib.h>
#include <string.h>

#include "all_matches.h"

typedef struct {
    FILE       *logfile;
    const char *text;

    ThreadManager *__manager;
} AllMatchesThreadManager;

/* --- ThreadManager function prototypes ------------------------------------ */

static void all_matches_thread_manager_reset(void *impl);
static void all_matches_thread_manager_free(void *impl);

static Thread *all_matches_thread_manager_spawn_thread(void       *impl,
                                                       const byte *pc,
                                                       const char *sp);
static void all_matches_thread_manager_schedule_thread(void *impl, Thread *t);
static void all_matches_thread_manager_schedule_thread_in_order(void   *impl,
                                                                Thread *t);
static Thread *all_matches_thread_manager_next_thread(void *impl);
static void    all_matches_thread_manager_notify_thread_match(void   *impl,
                                                              Thread *t);
static Thread *all_matches_thread_manager_clone_thread(void         *impl,
                                                       const Thread *t);
static void    all_matches_thread_manager_kill_thread(void *impl, Thread *t);
static void    all_matches_thread_manager_init_memoisation(void       *impl,
                                                           size_t      nmemo_insts,
                                                           const char *text);

static const byte *all_matches_thread_pc(void *impl, const Thread *t);
static void all_matches_thread_set_pc(void *impl, Thread *t, const byte *pc);
static const char *all_matches_thread_sp(void *impl, const Thread *t);
static void        all_matches_thread_inc_sp(void *impl, Thread *t);
static int         all_matches_thread_memoise(void *impl, Thread *t, len_t idx);
static cntr_t
all_matches_thread_counter(void *impl, const Thread *t, len_t idx);
static void
all_matches_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val);
static void  all_matches_thread_inc_counter(void *impl, Thread *t, len_t idx);
static void *all_matches_thread_memory(void *impl, const Thread *t, len_t idx);
static void  all_matches_thread_set_memory(void       *impl,
                                           Thread     *t,
                                           len_t       idx,
                                           const void *val,
                                           size_t      size);
static const char *const *
all_matches_thread_captures(void *impl, const Thread *t, len_t *ncaptures);
static void all_matches_thread_set_capture(void *impl, Thread *t, len_t idx);

/* --- API Routine ---------------------------------------------------------- */

ThreadManager *all_matches_thread_manager_new(ThreadManager *thread_manager,
                                              FILE          *logfile,
                                              const char    *text)
{
    AllMatchesThreadManager *amtm = malloc(sizeof(*amtm));
    ThreadManager           *tm   = malloc(sizeof(*tm));

    amtm->logfile   = logfile;
    amtm->text      = text;
    amtm->__manager = thread_manager;

    THREAD_MANAGER_SET_ALL_FUNCS(tm, all_matches);
    tm->impl = amtm;

    return tm;
}

/* --- ThreadManager functions ---------------------------------------------- */

static void print_match(AllMatchesThreadManager *self, Thread *t)
{
    const char  *capture;
    const char **captures;
    len_t        i, ncaptures;
    int          ncodepoints;

    if (!t) return;

    fprintf(self->logfile, "matched = TRUE\n");
    fprintf(self->logfile, "captures:\n");
    thread_manager_captures(self->__manager, t, &ncaptures);
    captures = malloc(2 * ncaptures * sizeof(const char *));
    memcpy(captures, thread_manager_captures(self->__manager, t, &ncaptures),
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
    thread_manager_reset(self->__manager);
}

static void all_matches_thread_manager_free(void *impl)
{
    AllMatchesThreadManager *self = impl;
    thread_manager_free(self->__manager);
    free(impl);
}

static Thread *all_matches_thread_manager_spawn_thread(void       *impl,
                                                       const byte *pc,
                                                       const char *sp)
{
    AllMatchesThreadManager *self = impl;

    return thread_manager_spawn_thread(self->__manager, pc, sp);
}

static void all_matches_thread_manager_schedule_thread(void *impl, Thread *t)
{
    thread_manager_schedule_thread(
        ((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_manager_schedule_thread_in_order(void   *impl,
                                                                Thread *t)
{
    thread_manager_schedule_thread_in_order(
        ((AllMatchesThreadManager *) impl)->__manager, t);
}

static Thread *all_matches_thread_manager_next_thread(void *impl)
{
    AllMatchesThreadManager *self = impl;
    Thread                  *t    = thread_manager_next_thread(self->__manager);

    return t;
}

static void all_matches_thread_manager_notify_thread_match(void   *impl,
                                                           Thread *t)
{
    AllMatchesThreadManager *self = impl;

    print_match(self, t);
    thread_manager_kill_thread(self->__manager, t);

    // thread_manager_notify_thread_match(self->__manager, t);
}

static Thread *all_matches_thread_manager_clone_thread(void         *impl,
                                                       const Thread *t)
{
    return thread_manager_clone_thread(
        ((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_manager_kill_thread(void *impl, Thread *t)
{
    AllMatchesThreadManager *self = impl;
    thread_manager_kill_thread(self->__manager, t);
}

static const byte *all_matches_thread_pc(void *impl, const Thread *t)
{
    return thread_manager_pc(((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_set_pc(void *impl, Thread *t, const byte *pc)
{
    thread_manager_set_pc(((AllMatchesThreadManager *) impl)->__manager, t, pc);
}

static const char *all_matches_thread_sp(void *impl, const Thread *t)
{
    return thread_manager_sp(((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_inc_sp(void *impl, Thread *t)
{
    thread_manager_inc_sp(((AllMatchesThreadManager *) impl)->__manager, t);
}

static void all_matches_thread_manager_init_memoisation(void       *impl,
                                                        size_t      nmemo_insts,
                                                        const char *text)
{
    thread_manager_init_memoisation(
        ((AllMatchesThreadManager *) impl)->__manager, nmemo_insts, text);
}

static int all_matches_thread_memoise(void *impl, Thread *t, len_t idx)
{
    return thread_manager_memoise(((AllMatchesThreadManager *) impl)->__manager,
                                  t, idx);
}

static cntr_t all_matches_thread_counter(void *impl, const Thread *t, len_t idx)
{
    return thread_manager_counter(((AllMatchesThreadManager *) impl)->__manager,
                                  t, idx);
}

static void
all_matches_thread_set_counter(void *impl, Thread *t, len_t idx, cntr_t val)
{
    thread_manager_set_counter(((AllMatchesThreadManager *) impl)->__manager, t,
                               idx, val);
}

static void all_matches_thread_inc_counter(void *impl, Thread *t, len_t idx)
{
    thread_manager_inc_counter(((AllMatchesThreadManager *) impl)->__manager, t,
                               idx);
}

static void *all_matches_thread_memory(void *impl, const Thread *t, len_t idx)
{
    return thread_manager_memory(((AllMatchesThreadManager *) impl)->__manager,
                                 t, idx);
}

static void all_matches_thread_set_memory(void       *impl,
                                          Thread     *t,
                                          len_t       idx,
                                          const void *val,
                                          size_t      size)
{
    thread_manager_set_memory(((AllMatchesThreadManager *) impl)->__manager, t,
                              idx, val, size);
}

static const char *const *
all_matches_thread_captures(void *impl, const Thread *t, len_t *ncaptures)
{
    return thread_manager_captures(
        ((AllMatchesThreadManager *) impl)->__manager, t, ncaptures);
}

static void all_matches_thread_set_capture(void *impl, Thread *t, len_t idx)
{
    thread_manager_set_capture(((AllMatchesThreadManager *) impl)->__manager, t,
                               idx);
}
