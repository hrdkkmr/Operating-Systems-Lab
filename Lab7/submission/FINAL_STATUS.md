# Final status

Legend: **PASS** = implemented, tested, and the test passed (log named).
**IMPL** = implemented but not tested by a dedicated test.
**LIMIT** = known limitation, documented.

## Q1 — Threads

| Requirement | Status | Evidence |
|---|---|---|
| `clone(fcn, arg, stack)` shares the address space; own kernel stack and context; starts at `fcn(arg)` on the user stack | PASS | 03/04/05: thread reads and writes the creator's globals; counter shared |
| Returns tid, or -1 on failure; validates the stack (non-null, page aligned, inside memory) and `fcn` | PASS | 03: 4 invalid-argument checks |
| Allocation-failure cleanup (kernel stack freed, slot returned) | IMPL | Code path in `clone()`. The proc table was never exhausted on purpose, but the 40-round leader-exit test would fail if slots leaked. |
| `join()` waits for a terminated thread and frees it; -1 without threads | PASS | 03 |
| `wait()` does not reap threads | PASS | 03: `wait()` returns -1, then `join()` collects the thread |
| Shared address space not freed prematurely (leader ownership, `reap_threads` in exit/exec) | PASS | 03: 40 × leader exit with a spinning and a blocked thread; `usertests` (10, 11) |
| `fork()` independent address space | PASS | 03 |
| `exec()` by a thread cannot invalidate others' address space | PASS | 03: `exec` from a thread returns -1. A leader's exec reaping its threads is IMPL. |
| `sbrk` from threads keeps `sz` consistent | IMPL | Serialized `growproc`; no dedicated concurrent-sbrk test. LIMIT: no TLB shootdown on shrink. |
| `threadtest`: 3 threads × 100,000, join, print counter; nondeterminism explained | PASS | 03/04/05, REPORT §1 |

## Q2 — Semaphores

| Requirement | Status | Evidence |
|---|---|---|
| 10 kernel semaphores, spinlock per semaphore, `sleep`/`wakeup`, no busy waiting, no lost wakeup | PASS | 06; design in REPORT §2 |
| Invalid ids, negative value, re-init while in use refused, documented return values | PASS | 06: 9 API checks; `semaphore.c` header comment |
| `synctest` A: semaphore counter = 300,000; comparison with Q1 | PASS | 06: 300000 vs 159990 |
| `synctest` B: bounded buffer 5, `empty` / `full` / `mutex`, finite items, no overflow or underflow, termination, logs | PASS | 06: 20/20 items, FIFO, max 5, ends 0 |

## Q3 — Deadlock

| Requirement | Status | Evidence |
|---|---|---|
| `detect_deadlock` with Allocation / Request / Available, iteration printout, deadlocked vs waiting | PASS | 07: scenarios 1 and 2a |
| `is_safe_state` (Banker's) with Need; safe sequence | PASS | 07: scenario 2b |
| Hypothetical request: validate, tentatively grant, check, keep or roll back | PASS | 07: 2c granted, 3 unsafe and rolled back, must-wait, over-claim |
| ≥ 3 labelled scenarios (deadlocked; all finish; safe → unsafe) | PASS | 07 |
| Input validation | PASS | 07: 3 checks |
| Real-process circular wait with pipes and shared memory, watchdog, cannot hang the shell | PASS | 07: cycle A → B → A, no progress for 100 ticks, recovery by kill, shell responsive |
| Kernel-level automatic inspection of pipe waits | Not implemented (not required) | Stated in REPORT §3 |

## Q4 — Priority scheduling

| Requirement | Status | Evidence |
|---|---|---|
| Priority field, range 1–20, default 10 | PASS | 08/09: API checks |
| `set_priority` / `get_priority` with validation | PASS | 08/09 |
| Scheduler picks the lowest numeric priority | PASS | 08/09: first-run times follow priority |
| Round robin among equal priorities (deterministic, not lowest slot) | PASS | 08/09: finish spread 6 / 37 ticks |
| Aging: > 30 ticks RUNNABLE → priority − 1, reset, floor 1; only RUNNABLE ages | PASS | 08: steps every ~31 ticks; 09: first runs at 152/308/464 |
| Priority 20 eventually runs | PASS | 08: t=310; 09: t=464 |
| Synchronized with `ptable.lock`; integrated with fork/exit | PASS | `usertests` 10/11; all programs (12) |
| `priotest`: four children incl. 20, completion order and times, aging events, summary | PASS | 08/09 |

## Integration and regression

| Requirement | Status | Evidence |
|---|---|---|
| Unique syscall numbers 22–29, dispatch entries, user stubs, prototypes | PASS | build 01; every call exercised |
| Programs registered in `UPROGS` and callable from the shell | PASS | 02 (`ls`) and 03–12 |
| Clean build | PASS | 01 (exit 0; only pre-existing warnings in untouched files) |
| Existing functionality (`usertests`, `forktest`, Lab 5 programs) | PASS | 02, 10, 11, 12 |
| Repeated execution without exhausting slots or memory | PASS | 03 (10 + 10 runs), 12 |
| Submission archive | PASS | `../submission.zip` (built by `make_submission.sh`) |
| Screenshots | PASS (emulator screen dumps) | `screenshots/`. Terminal-window screenshots must be taken by hand: `SCREENSHOT_CHECKLIST.md` |
