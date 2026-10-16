# Tasks

- [x] [core] [needs-tests] Repair registry lifetime and cv root identity.
- [x] [tests] Validate and freeze corrected V1, including actual process exit,
  Linux P2996, sanitizers, TSan, and original-format interchange.
- [x] [core] [tests] Remove repeated load initialization and save payload copy.
- [x] [core] [tests] Replace linear owner lookup and test actual node/rehash failures.
- [x] [core] [tests] Validate live-tail expansion and fallback behavior.
- [x] [tests] Run final cross-version mutation, failure and sanitizer checks.
- [x] [tests] Compare individual stages and combination with portable evidence.
- [x] [docs] Record decisions on optional policies and update migration/status.

Evidence: `docs/VM_STORAGE_IMPROVEMENTS_RESULTS.md` records final test counts,
individual/combined performance, the repeated small-load regression, and
optional-policy decisions. Windows compilation/execution remains unavailable;
no pool, physical reclamation, Automatic growth, or freelist rewrite is claimed.
No commit, push, package publication or remote CI was performed.
