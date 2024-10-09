#include <stdlib.h>

#include "all_matches.h"

typedef struct {
    FILE       *logfile; /**< the file stream for logging captures on match   */
    const char *text;    /**< the input string being matched against          */
} BruAllMatchesThreadManager;

/* --- AllMatchesThreadManager function prototypes -------------------------- */

static void all_matches_thread_manager_free(BruThreadManager *tm);
static void all_matches_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                           BruThread        *t);

/* --- API function definitions --------------------------------------------- */

BruThreadManager *bru_all_matches_thread_manager_new(BruThreadManager *tm,
                                                     FILE             *logfile,
                                                     const char       *text)
{
    BruAllMatchesThreadManager *amtm = malloc(sizeof(*amtm));
    BruThreadManagerInterface  *tmi, *super;

    amtm->logfile = logfile;
    amtm->text    = text;

    super = bru_vt_curr(tm);
    tmi   = bru_thread_manager_interface_new(amtm, super->_thread_size);
    tmi->notify_thread_match = all_matches_thread_manager_notify_thread_match;
    tmi->free                = all_matches_thread_manager_free;

    // NOLINTNEXTLINE(bugprone-sizeof-expression)
    bru_vt_extend(tm, tmi);

    return tm;
}

/* --- AllMatchesThreadManager function definitions ------------------------- */

static void print_match(BruThreadManager *tm, BruThread *t)
{
    BruAllMatchesThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *curr = bru_vt_curr(tm);
    const char *const          *captures;
    bru_len_t                   i, ncaptures;
    int                         ncodepoints;

    if (!t) return;

    fprintf(self->logfile, "matched = TRUE\n");
    fprintf(self->logfile, "captures:\n");
    bru_vt_call_super_function(tm, curr, captures, captures, t, &ncaptures);
    fprintf(self->logfile, "  input: '%s'\n", self->text);
    for (i = 0; i < 2 * ncaptures; i += 2) {
        fprintf(self->logfile, "%7hu: ", i);
        if (captures[i]) {
            ncodepoints = stc_utf8_str_ncodepoints(self->text) -
                          stc_utf8_str_ncodepoints(captures[i]);
            fprintf(self->logfile, "%.*s'%.*s'\n", ncodepoints, "",
                    (int) (captures[i + 1] - captures[i]), captures[i]);
        } else {
            fprintf(self->logfile, "not captured\n");
        }
    }
}

static void all_matches_thread_manager_free(BruThreadManager *tm)
{
    BruAllMatchesThreadManager *self = bru_vt_curr_impl(tm);
    BruThreadManagerInterface  *tmi  = bru_vt_curr(tm);

    bru_vt_shrink(tm);
    bru_vt_call_procedure(tm, free);

    free(self);
    bru_thread_manager_interface_free(tmi);
}

static void all_matches_thread_manager_notify_thread_match(BruThreadManager *tm,
                                                           BruThread        *t)
{
    print_match(tm, t);
    bru_vt_call_super_procedure(tm, bru_vt_curr(tm), notify_thread_match, t);

    // thread_manager_notify_thread_match(self->__manager, t);
}
