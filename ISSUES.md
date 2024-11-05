# Record of known issues in BRU

Issues in this file are listed as a checkbox list:
- [ ] Short description (max 60 characters)

  **File(s):** File path(s) relative to `./src`.  
  **Location:** If known, somewhere within the file.  
  **Date reported:** Date of reporting (yyyy-mm-dd).  
  **Reported by:** Means of identification.  
  **Relevant branch:** The branch to be used for resolving the issue.

  Long description

Using line and column numbers to specify the location within the file is not  
recommended, as any edits to the files will likely make them useless.

If you have fixed an issue, check the list box with an `x`, and append the
fields **Date fixed** and **Fixed by** after **Relevant branch**.

## Issues

- [x] Memory leak in lockstep scheduler

  **File:** `vm/thread_managers/schedulers/lockstep.c`  
  **Location:** function `lockstep_scheduler_next`  
  **Date reported:** 2024-10-15  
  **Reported by:** [aroodt][aroodt]  
  **Relevant branch:** `fix/lockstep-scheduler`  
  **Date fixed:** 2024-10-16  
  **Fixed by:** [bkmwatling][bkmwatling]

  When requesting the next thread from the scheduler, there is a chance the
  thread points to a `CHAR`/`PRED` instruction. If the scheduler is not
  currently in lockstep then it attempts to reschedule the thread using
  `lockstep_scheduler_schedule`. If this call returns FALSE, the thread is
  supposed to be killed since it is a duplicate—this does not currently happen,
  which results in the thread's memory being lost.

- [ ] Switch to using `stdbool` for Boolean values

  **File:** Whole project  
  **Location:** Whole project  
  **Date reported:** 2024-10-18  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `refactor/stdbool`

  The project uses integers to represent Boolean values with the `TRUE` and
  `FALSE` macros defined in `src/types.h`. This is unnecessary as the project
  requires the C11 standard or later that definitely supports `stdbool.h` which
  is more portable and is better practice to use. As such, the project should be
  ported to use `stdbool` where necessary.

- [ ] Change Spencer thread manager and scheduler to backtrack

  **Files:** `vm/thread_managers/spencer.[ch]` and
    `vm/thread_mangers/schedulers/spencer.[ch]`  
  **Location:** Whole files  
  **Date reported:** 2024-10-18  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `refactor/spencer-to-backtrack`

  For symmetry, the _Spencer_ thread manager and scheduler should be changed to
  _backtrack_ similar to how the _Thompson_ thread manager and scheduler have
  been named _lockstep_. This name change is better than changing the _lockstep_
  thread manager and scheduler back to _Thompson_ as there is also a _Thompson_
  construction, which would introduce ambiguities.

- [ ] Perform validation on transition and state identifiers in smir.c

  **File:** `fa/smir.c`
  **Location:** Functions operating on bru_trans_id and bru_state_id
  **Date reported:** 2024-11-01
  **Reported by:** [aroodt][aroodt]
  **Relevant branch:** `fix/smir-id-validation`

  State and transition identifiers should be validated prior to use, as there is
  no guarantee they will be valid. I recommend simple functions for checking
  validity of each identifier should be good enough.

- [ ] Provide list of transformations at compile time

  **File:** `vm/compile.c`
  **Location:** function `compiler_compile`
  **Date reported:** 2024-11-01
  **Reported by:** [aroodt][aroodt]
  **Relevant branch:** `feature/transformer-compile-sequence`

  The current compilation pipeline is hardcoded in `compile.c`, but it would be
  convenient and user-friendly to allow providing a list of transformations
  to be made on the constructed SMIR. Possibly this should be provided in a
  separate function entirely. One could maybe use variadic functions, with NULL
  indicating the list of transforms is finished. This will require defining a
  struct to contain each transform and any arguments required outside of the
  state machine being transformed. The intermediate state machines should also
  be appropriately free'd. In my opinion, the original state machine and the
  final state machine should be the only state machines left malloc'd (and they
  may be the same state machine if the transforms never created a new one).

<!-- NOTE: links to profile webpages associated with your slug/identifier -->
[aroodt]: https://www.github.com/aroodt
[bkmwatling]: https://www.gitlab.com/bkmwatling
