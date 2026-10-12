# Testing — OS Lab Assignment 7

## Environment and method

* Windows 11, WSL2 Ubuntu, gcc 15.2.0, GNU make, QEMU 10.2.1
  (`qemu-system-i386`, TCG emulation, **no KVM**), SeaBIOS 1.17.
* Each session boots xv6 headless with `tools/run_xv6.py`. The harness types
  the commands at the `$` prompt, enforces a timeout per command, and kills
  QEMU at the end, so no test can hang the session.
* `output_logs/NN_*.log` hold the raw serial console of each session, byte
  for byte, including the BIOS escape sequences at the top.
  `output_logs/harness_notes.txt` gives the wall time and exit status of
  every command.
* `screenshots/NN_*/` are PNG dumps of the emulated VGA screen after each
  command, showing its last 25 lines.
* The logs were captured from the final sources: kernel after the last
  change, user programs after the last change. Earlier exploratory runs are
  not included.
* One tick = 10 ms of xv6 time. Emulation speed varies with host load, so
  tick counts differ between runs. The *patterns* below were stable across
  every run I made.

## Baseline

| Test | Command | Result |
|---|---|---|
| Build of the starting commit `54490cf` | `make xv6.img fs.img` | **Failed**: `undefined reference to 'semaphore_init'`, plus `warning: 'p' is used uninitialized` in `allocproc` (`00_baseline_build_failure.log`). So there was no working baseline to boot. |
| Clean build of the final tree | `make clean && make xv6.img fs.img` | Exit status 0 (`01_clean_build.log`). The only compiler warnings are in files this lab did not touch (`mp.c` `-Warray-bounds` and `sh.c` `-Winfinite-recursion` under gcc 15) and the linker's RWX-segment notices. |
| Boot, 2 CPUs; shell | `ls`, `echo`, `cat README \| wc`, `forktest` | `cpu1: starting 1`, `cpu0: starting 0`. All commands work. `fork test OK` (`02_…`). |

### Environment problems found and fixed while testing

1. **xv6 ran on one CPU.** With a plain `-smp 2`, SeaBIOS lists one MP-table
   processor entry per socket, and QEMU 10 puts all CPUs in one socket.
   Measured with a temporary debug print (since removed): `ncpu=1`; table
   entry types `0 1 1 2 3…`. With `-smp 2,sockets=2,cores=1,threads=1`:
   `ncpu=2`, two type-0 entries, and `cpu1: starting 1`. The Makefile was
   fixed. The Lab 5 output in this repository also shows only `cpu0`.
2. **`usertests` stalled on 2 CPUs.** Once SMP actually worked, `usertests`
   stalled in `linkunlink` or `bigdir`. The process was in state `run`, not
   asleep. Sampling EIP through the QEMU monitor showed both CPUs almost
   always in `lapicid()` (`mycpu()` → LAPIC MMIO read) or in `acquire()`:
   the idle CPU's busy scheduler loop was hammering emulated MMIO. Fix:
   when nothing is runnable, the idle CPU runs `sti; hlt`. Afterwards
   `usertests` passed in 147 s (2 CPUs) and 153 s (4 CPUs).
3. **A test-harness false timeout.** Once in about 20 runs of `zombie`, the
   shell's prompt was printed inside init's `zombie!` message (`zomb$i e!`),
   because two processes write to the console one character at a time. The
   kernel was idle and healthy: Ctrl-P showed `init` and `sh` sleeping
   normally. The harness now falls back to an `echo __done_N__` marker
   after 120 s of silence. Verified with 15 consecutive `zombie` runs, two
   of which interleaved.

## Q1 — threads (`threadtest`)

| Case | Session | Result |
|---|---|---|
| 3 threads created, joined, final counter printed | 03 | `created thread 0 (tid 4) … joined tid 4 …` |
| Unsynchronized counter, 2 CPUs, start barrier, 10 runs | 03 | **6 of 10 runs lost updates**; values 213814 to 300000 |
| Same, no barrier (`-n`) | 03 | 1 of 10 runs lost updates (266134) |
| 4 CPUs, 5 runs | 05 | 2 of 5 lost updates (108449, 240953) |
| 1 CPU, 5 runs | 04 | 0 of 5: always 300000 (see the report for why this is expected under TCG) |
| Thread reads and writes creator's globals (41 + 1 = 42) | 03–05, 12 | PASS |
| `clone` rejects misaligned stack, stack outside address space, null stack, entry point outside address space | 03–05, 12 | PASS (returns -1) |
| `join` with no threads → -1 | 03–05, 12 | PASS |
| `wait()` ignores a finished thread (returns -1); `join()` then collects it | 03–05, 12 | PASS |
| `fork()` child's write invisible to the parent | 03–05, 12 | PASS |
| `exec()` from a thread → -1 | 03–05, 12 | PASS |
| 40 × leader exits without `join` while one thread spins and one is blocked in `sem_wait` | 03–05, 12 | PASS. All reaped; a leak would exhaust the 64 proc slots |
| Repeated execution | 03, 12 | 10 + 10 runs in one boot, plus two more `threadtest 5` in the regression session. No failures |

## Q2 — semaphores (`synctest`)

Session 06 (2 CPUs) and session 12.

| Case | Result |
|---|---|
| Same three-thread counter, no synchronization | 159990 of 300000 (17 ticks) |
| With `sem_wait`/`sem_signal` around every increment (mutex = 1) | **300000 of 300000** (5133 ticks); PASS |
| Invalid ids (-1, 10), negative initial value, invalid ids for wait/signal | PASS (-1) |
| `sem_wait` on value 0 blocks; `sem_init` on a semaphore with a waiter is refused; `sem_signal` wakes the waiter, whose `sem_wait` returns 0; idle semaphore can be re-initialized | PASS (5 checks) |
| Producer/consumer, buffer 5, 20 items | produced 20, consumed 20, sum 210, FIFO order, occupancy max 5 and final 0. The buffer sat at `5/5` while the consumer was slow and at `0/5` while the producer was slow. 4 × PASS |
| Termination | the program exits; prompt returns |

Earlier runs on 4 CPUs (exploratory, before the final build) gave
`117200` (unsynchronized) and `300000` (synchronized, 14264 ticks).

## Q3 — deadlock (`deadlock`)

Session 07 (`deadlock matrix`, `deadlock procs`) and session 12 (`deadlock`).
The expected results were worked out by hand beforehand. The program
computes them at run time.

| Scenario | Result |
|---|---|
| 1. Detection; Allocation and Request of the textbook example with P2 requesting (0 0 1), Available (0 0 0) | P0 finishes (Work becomes (0 1 0)); **P1–P4 DEADLOCKED** — PASS |
| 2a. Same, P2's request (0 0 0) | Order P0, P2, P3, P4, P1; Work ends at (7 2 6). P1/P3/P4 reported as "waiting, NOT deadlocked" — PASS |
| 2b. Banker, Available (3 3 2) | Need printed; **safe sequence <P1 P3 P4 P0 P2>** — PASS |
| 2c. P1 requests (1 0 2) | tentatively granted, safe → **GRANTED**, Available (2 3 0) — PASS |
| 3. State after 2c is safe | PASS |
| 3. P0 requests (0 2 0) | within Need and Available, but Work (2 1 0) satisfies no Need → **UNSAFE, rolled back**; matrices identical afterwards — PASS |
| 3. P4 requests (3 3 0) | more than Available → must wait — PASS |
| 3. P4 requests (0 0 3) | more than Need (4 3 1) → rejected — PASS |
| Input validation | dimensions 0, > MAXP, > MAXR; negative counts; Allocation > Max → -1 — PASS |
| Real-process circular wait | A (pid 5) and B (pid 6) both report "waiting"; after 100 more ticks both are still blocked; wait-for graph A → B → A gives a cycle. Killing victim A closes its pipe write end, so B reads EOF and exits normally. Both are reaped — 2 × PASS |
| Shell not hung | `echo shell still responsive` works afterwards |

## Q4 — scheduler (`priotest`)

| Case | 2 CPUs (session 08) | 1 CPU (session 09) |
|---|---|---|
| Out-of-range priority, bad pid, default 10, exited pid → -1 | 6 × PASS | 6 × PASS |
| Aging events (traced by the parent with `get_priority`) | 15, every ~31 ticks (31, 63, 96, 128, 156, …) | 30 |
| Priority 5: first run / finish | t=0 / t=262 | t=0 / t=603 |
| Priority 10 | t=1 / t=336 (stays 10) | t=152 / t=1003 (aged to 5) |
| Priority 15 | t=155 / t=493 (aged to 10) | t=308 / t=1145 (aged to 5) |
| Priority 20 | **t=310** / t=598 (aged to 10) | **t=464** / t=1206 (aged to 5) |
| Equal priority 15 × 3: finish spread | 6 ticks (423–429) | 37 ticks (849–886) |
| All checks | PASS | PASS |

A fairness bug was found and fixed with this test. The first round-robin
version (a "next slot" cursor) gave finish times of 24, 27 and **47** ticks
for three equal-priority children, because the periodically waking
priority-1 parent kept resetting the cursor. The final version
(least-recently-dispatched) gives the spreads above.

## Regression

| Test | Result |
|---|---|
| `usertests`, fresh `fs.img`, 2 CPUs (session 10) | **ALL TESTS PASSED** (146.6 s) |
| `usertests`, fresh `fs.img`, 4 CPUs (session 11) | **ALL TESTS PASSED** (152.5 s) |
| All Lab 7 programs again in one boot, twice where useful, then the Lab 5 programs `prodcons`, `readwrite`, `dining`, `peterson`, and `zombie`, `forktest`, `ls` (session 12) | All completed. 6 × `ALL CHECKS PASSED`, `Producer-Consumer completed successfully`, `Readers-Writers completed successfully`, `Dining Philosophers completed successfully`, `Final counter = 20 (expected 20)`, `zombie!`, `fork test OK`. Process slots were not exhausted. |

## Not tested / limitations

* No test on real hardware or under KVM; all results are from TCG.
* On 1 CPU the unsynchronized counter never lost an update under TCG, so the
  race is demonstrated only with ≥ 2 CPUs.
* Concurrent `sbrk()` shrinking by one thread while another runs on a
  different CPU is not tested; xv6 has no TLB shootdown (see report).
* The `dining` output is character-interleaved on SMP, as it would be with
  any program whose processes print concurrently. The program's own
  completion summary is correct.
* `screenshots/` are emulator screen dumps, not photos of a terminal window.
  See `SCREENSHOT_CHECKLIST.md` for taking those by hand.
