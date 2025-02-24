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

  **Files:** Whole project  
  **Location:** Whole project  
  **Date reported:** 2024-10-18  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `refactor/stdbool`

  The project uses integers to represent Boolean values with the `TRUE` and
  `FALSE` macros defined in `src/types.h`. This is unnecessary as the project
  requires the C11 standard or later that definitely supports `stdbool.h` which
  is more portable and is better practice to use. As such, the project should be
  ported to use `stdbool` where necessary.

- [x] Change Spencer thread manager and scheduler to backtrack

  **Files:** `vm/thread_managers/spencer.[ch]` and
    `vm/thread_mangers/schedulers/spencer.[ch]`  
  **Location:** Whole files  
  **Date reported:** 2024-10-18  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `refactor/spencer-to-backtrack`
  **Date fixed:** 2025-02-24  
  **Fixed by:**  [bkmwatling][bkmwatling]

  For symmetry, the _Spencer_ thread manager and scheduler should be changed to
  _backtrack_ similar to how the _Thompson_ thread manager and scheduler have
  been named _lockstep_. This name change is better than changing the _lockstep_
  thread manager and scheduler back to _Thompson_ as there is also a _Thompson_
  construction, which would introduce ambiguities.

- [ ] Cleanup regex parser

  **Files:** `re/parser.[ch]`  
  **Location:** Whole files  
  **Date reported:** 2024-10-29  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `refactor/regex-parser`

  The regex parser has many _questionable_ add-ons which were implemented
  quickly due to time constraints of reports/papers. This has lead to the parser
  being difficult to understand or extend (e.g. adding state for parsed subtrees
  needed for counters). Thus, it _should_ be refactored for readability and
  extensibility.

- [ ] Move SMIR compiled `memoset` instructions to end of program

  **File:** `fa/smir.c`  
  **Location:** `compile` functions  
  **Date Reported:** 2024-10-29  
  **Reported by:** [aroodt][aroodt]  
  **Relevant branch:** `refactor/memoset-compilation`

  Since the `memoset` instruction kills the executing thread, any instructions
  that follow it are "dead code". Thus, all compiled `memoset` instructions can
  be moved to the end of the program with jumps to relevant ones in the main
  program. This would remove clutter in the compiled program which is currently
  present due to them being compiled like any other transition.

- [ ] Remove epsilon-loop checks for non-nullable subexpressions

  **File:** `fa/constructions/thompson.c`  
  **Location:** `emit` function  
  **Date reported:** 2024-10-29  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `refactor/epsilon-loop`

  If a subexpression of a repetition is not nullable, there is no need to
  perform epsilon-loop checks. Since the regex parse tree contains information
  on the nullability of a subexpression, it is simple to remove unnecessary
  epsilon-loop checks.

- [ ] Add general SMIR transform to remove unnecessary actions

  **Files:** `fa/transformers/optimisation.[hc]`  
  **Location:** Whole files  
  **Date reported:** 2024-10-29  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `feature/smir-optimisation`

  During the flattening process, there are transitions that accumulate actions
  that make others obsolete. This could happen in general, and so a single
  optimisation transformation should be implemented to remove these useless
  actions.

- [ ] Perform validation on transition and state identifiers in `smir.c`

  **File:** `fa/smir.c`  
  **Location:** Functions operating on `bru_trans_id` and `bru_state_id`  
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

- [ ] Fix flatten transformation with counters

  **File:** `fa/transformers/flatten.c`  
  **Location:** Rework whole file  
  **Date reported:** 2024-11-17  
  **Reported by:** [bkmwatling][bkmwatling]  
  **Relevant branch:** `fix/flatten-counters`

  The flattening transformation does not take into account counter actions,
  mainly because it would potentially need to keep track of possible counter
  values to determine which flattened transitions to keep. However, it is
  planned that flattening won't prune transitions, and so this operation would
  be deferred to the optimisation transform. In any case this issue to be a
  reminder that counters don't work with the flatten transform.

- [x] Add instruction IR for better code optimisation

  **File:** `fa/smir.c`  
  **Location:** function `bru_smir_compile` and its derivatives  
  **Date reported:** 2025-01-23  
  **Reported by:** [aroodt][aroodt]  
  **Relevant branch:** `feature/instruction-ir`  
  **Date fixed:** 2025-02-05  
  **Fixed by:**  [aroodt][arroodt]

  Compilers for programming languages typically implement an intermendiate
  representation consisting of a sequence of tuples containing the bytecode and
  arguments, which allows for some optimisations. A clear optimisation worth
  exploring is identifying points of code duplication, allowing for code reuse.
  In this case, we could replace possibly large sequences of code with single
  JMP instructions, greatly reducing the size of the program. Such
  identification may be possible in the existing SMIR, but it would require
  more contemplation.

- [x] Move SMIR compilation algorithm to `/vm/`

  **File:** `fa/smir.c`  
  **Location:** function `bru_smir_compile` and its derivatives  
  **Date reported:** 2025-01-23  
  **Reported by:** [aroodt][aroodt]  
  **Relevant branch:** `refactor/smir-compilation`
  **Date fixed:** 2025-02-05  
  **Fixed by:**  [aroodt][arroodt]

  BRU's codebase structure gives clear separation between machinery used for
  parsing (the `re` folder), finite automata (the `fa` folder), and the virtual
  machine (the `vm` folder). However, while `fa` contains machinery for
  converting a parse tree (the result of using machinery in `re`) into a SMIR
  instance, the machinery for converting a SMIR instance into a Prog (used by
  the machinery in `vm`) is also contained within `fa`. For consistency, it
  would make more sense for this machinery to exist within `vm`.

- [ ] Add serialisation to BRU's VM code representation (Prog)

  **File:** `fa/program.c`  
  **Location:** function `bru_prog_serialise` (to be created)  
  **Date reported:** 2025-01-23  
  **Reported by:** [aroodt][aroodt]  
  **Relevant branch:** `feature/prog-serialisation`

  The ability to serialise a compiled program to an external file would
  facilitate faster, safer testing and data collection, allowing regexes to be
  compiled once and reused for multiple tests. This will also provide save
  points in the event of code crashing during data collection, meaning

- [x] Posix character classes not recognised during parsing

  **File:** `re/parser.c`  
  **Location:** function `parse_posix_cc`  
  **Date reported:** 2025-01-24  
  **Reported by:** [aroodt][aroodt]  
  **Relevant branch:** `fix/posix-cc`
  **Date fixed:** 2025-02-20  
  **Fixed by:**  [aroodt][arroodt]

  Posix character classes are not correctly recognised during parsing due to
  the use of `strcmp`. This causes classes like "[:alnum:]" to be compared with
  the remainder of the regex being parsed. This will never result in a match
  since, at minimum, the valid regex could be "[[:alnum:]]", resulting in a
  call `strcmp("[:alnum:]", "[:alnum:]]")`. While a prefix tree (possibly a
  radix tree) would be optimal in the general case, I think given how few posix
  character classes there are, switching to `strncmp` should suffice.


<!-- NOTE: links to profile webpages associated with your slug/identifier -->
[aroodt]: https://www.github.com/aroodt
[bkmwatling]: https://www.gitlab.com/bkmwatling
