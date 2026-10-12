# File manifest — OS Lab Assignment 7

All paths are relative to `Lab7/`. The xv6 source tree in `xv6/` is the
canonical, buildable copy. Files under `submission/src/` are copies made by
`submission/make_submission.sh` and must not be edited by hand.

Starting point: commit `54490cf` ("7th Lab Initial Commit" + merge). It
contained the Lab 5 code (shared page and semaphores) and a partial,
non-building Lab 7 attempt. **It did not build**: the kernel link failed with
`undefined reference to 'semaphore_init'`, and `allocproc()` wrote through an
uninitialized pointer (see `output_logs/00_baseline_build_failure.log`).

## Kernel files changed

| File | Change |
|---|---|
| `xv6/proc.h` | Documented `is_thread` (Q1). Added `last_sched` (Q4 round robin). Added `MIN_PRIORITY`, `MAX_PRIORITY`, `DEFAULT_PRIORITY` (10) and `AGING_THRESHOLD` (30). The `priority` and `wait_ticks` fields were already declared. |
| `xv6/proc.c` | Q1: `clone()`, `join()`, `freethread()`, `reap_threads()`. `exit()` reaps a leader's threads and gives an exiting thread's thread-children to the leader. `wait()` skips threads. `growproc()` serializes resizes and publishes `sz` to every thread. `allocproc()` initializes every new field under `ptable.lock`, fixing the uninitialized-pointer bug. `shm_get()` refuses threads. Q4: new `scheduler()` (strict priority plus least-recently-dispatched round robin), `age_runnable()`, `set_priority()`, `get_priority()`. `procdump()` (Ctrl-P) also shows priority and thread flag. |
| `xv6/x86.h` | Added `sti_hlt()`. `scheduler()` uses it to halt an idle CPU until the next interrupt. The original busy idle loop flooded emulated LAPIC MMIO (through `mycpu()` in `acquire()`) and made `usertests` appear hung on 2 CPUs. |
| `xv6/semaphore.c` | Rewritten (Q2): 10 semaphores, each with its own spinlock, a nonnegative count and a waiter count. Uses `sleep()`/`wakeup()`. Validates ids, values and initialization. Refuses to re-initialize while a process is blocked. A waiter that is killed returns -1. |
| `xv6/exec.c` | Q1: `exec()` from a non-leader thread returns -1. A leader kills and reaps its threads at the commit point. Also releases the Lab 5 shared page before freeing the old page table; previously `freevm()` could free a page that other processes still mapped. |
| `xv6/trap.c` | Q4: CPU 0's timer interrupt calls `age_runnable()` once per tick. |
| `xv6/sysproc.c` | Wrappers for `clone`, `join`, `sem_*`, `set_priority`, `get_priority` and `shm_get`. `clone` arguments are read with `argint`, because `arg` is opaque to the kernel. `sbrk` gets its old size from `growproc()` under the lock. |
| `xv6/syscall.h`, `xv6/syscall.c`, `xv6/usys.S` | Syscall numbers 22–29, dispatch entries and user stubs. `SYS_shm_get` (29) was missing; without it the Lab 5 programs could not link. |
| `xv6/defs.h` | Prototypes fixed: `growproc(int, uint*)`, `seminit`, `reap_threads`, `age_runnable`. Removed duplicates and the stale `semaphore_init`/`update_aging` prototypes. |
| `xv6/user.h` | Removed duplicate `sem_*` prototypes. Declares all Lab 7 calls. |
| `xv6/param.h` | `FSSIZE` raised from 1000 to 2000 blocks. With 4 more programs the user binaries total 483,596 bytes, which no longer fit in a 1000-block `fs.img`. |
| `xv6/Makefile` | The four programs were added to `UPROGS`. `QEMUOPTS` now uses `-smp N,sockets=N,cores=1,threads=1`. With QEMU 10.2 + SeaBIOS 1.17 a plain `-smp N` puts all CPUs in one socket, the BIOS MP table then lists one CPU, and xv6 silently runs on **one** CPU. |

Unchanged: `vm.c`, `mp.c`, `main.c` and all file-system, driver and lock code.

## User programs (Lab 7)

| File | Purpose |
|---|---|
| `xv6/threadtest.c` | Q1. Three threads × 100,000 unsynchronized increments (optional repeat count, `-n` = no start barrier). Followed by 11 checks: sharing, argument validation, `wait` vs `join`, fork independence, exec from a thread, 40× leader exit with live threads. |
| `xv6/synctest.c` | Q2. Unsynchronized vs semaphore-protected counter, semaphore API checks, bounded-buffer producer/consumer (5 slots, 20 items) with logs and invariant checks. |
| `xv6/deadlock.c` | Q3. Deadlock detection, Banker's safety and request algorithms, three scenarios with intermediate state, input validation, and a two-process pipe circular wait with a watchdog and recovery. |
| `xv6/priotest.c` | Q4. Priority getter/setter checks. Experiment 1: priorities 5/10/15/20 with live aging trace. Experiment 2: three equal priorities (round-robin fairness). |

Pre-existing Lab 5 programs (`peterson`, `prodcons`, `readwrite`, `dining`)
are unchanged and still build and run.

## Submission folder

| Path | Content |
|---|---|
| `submission/README.md` | Build, boot and run instructions. |
| `submission/REPORT.md` | The report. |
| `submission/TESTING.md` | Test cases, commands, actual results, limitations. |
| `submission/FINAL_STATUS.md` | Requirement-by-requirement status. |
| `submission/SCREENSHOT_CHECKLIST.md` | Screenshots to take and how. |
| `submission/src/` | Copies of the four test programs and of the changed kernel files, plus `kernel.patch` (diff against commit `54490cf`). |
| `submission/output_logs/` | Raw serial-console logs of the test runs, as captured. |
| `submission/screenshots/` | PNG screen captures of the emulated VGA console (QEMU `screendump`). |
| `submission/tools/run_xv6.py` | The harness that produced the logs and screenshots. |
| `submission/make_submission.sh` | Regenerates `src/` and the zip from the canonical sources. |
