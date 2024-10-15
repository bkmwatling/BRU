# Record of known issues in BRU

Issues in this file are listed as a checkbox list:  
- [ ] Short description (max 60 characters)

    **File:** File path relative to `./src`.  
    **Location:** If known, somewhere within the file.  
    **Date reported:** Date of reporting.  
    **Reported by:** Means of identification.

    Long description

Using line and column numbers to specify the location within the file is not  
recommended, as any edits to the files will likely make them useless.

If you have fixed an issue, check the list box with an `x`, and append the
fields **Date fixed:** and **Fixed by:** after **Reported by:**.

## Issues

- [ ] Memory leak in lockstep scheduler

    **File:** `vm/thread_managers/schedulers/lockstep.c`  
    **Location:** function `lockstep_scheduler_next`  
    **Date reported:** 2024-10-15  
    **Reported by:** [aroodt][aroodt]

    When requesting the next thread from the scheduler, there is a chance the thread
    points to a CHAR/PRED instruction. If the scheduler is not currently in lockstep
    then it attempts to reschedule the thread using `lockstep_scheduler_schedule`.
    If this call returns FALSE, the thread is supposed to be killed since it is a
    duplicate -- this does not currently happen, which results in the thread's
    memory being lost.

<!-- NOTE: links to profile webpages associated with your slug/identifier -->
[aroodt]: https://www.github.com/aroodt
