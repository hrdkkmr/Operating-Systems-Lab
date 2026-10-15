# Testing — Lab 8

## Environment

- Windows 11, WSL distro `Ubuntu`: gcc 15.2.0, GNU make, `qemu-system-i386`, python3.
- QEMU options as in the Makefile: `-smp 2 -m 512`, `fs.img` + `xv6.img`. No graphical window: the serial console is used and the VGA screen is captured through the QEMU monitor.

## Reproducing

From `Lab8/xv6` in Linux/WSL:

```sh
make clean
make fs.img xv6.img          # build (kernel + all user programs)
sh runlab8.sh                # all tests -> ../submission/output_logs, ../submission/screenshots
```

Manual, interactive run:

```sh
make qemu-nox                # or: make qemu
$ vatest
$ memtest
$ vmprinttest
$ lazytest
$ lazytest oom
$ cowtest
$ protecttest
$ shmtest
$ pagerepl
$ pagerepl 3 1 2 3 4 1 2 5
```

To leave QEMU: `Ctrl-a x`.

`runxv6.py LOG [--timeout S] [--shots DIR] CMD...` boots QEMU, waits for `$ `, types each command, waits for the next prompt, then kills QEMU. A hung or faulting test therefore can never leave QEMU running. Commands time out after 900 s in `runlab8.sh`; none came close.

## Results

All results come from the runs whose logs are in `output_logs/`.

| Step | Command(s) | Result | Log |
|---|---|---|---|
| Baseline build (unmodified Lab 7 tree) | `make` | built; only pre-existing warnings | (console) |
| Baseline boot | `ls`, `echo`, `forktest` | OK | `baseline/baseline.log` |
| Baseline regression | `usertests`, `threadtest`, `synctest` | ALL TESTS PASSED / ALL CHECKS PASSED | `baseline/baseline_usertests.log` |
| Clean build after changes | `make clean; make fs.img xv6.img` | success; no warnings from Lab 8 code (only pre-existing `sh.c`, `mp.c` and linker RWX warnings) | `00_clean_build.log` |
| Q1 | `vatest` | 11/11 checks pass | `q1_vatest.log` |
| Q2 | `memtest` (twice in one boot) | 20/20 pass; baseline free pages identical in both runs | `q2_memtest.log` |
| Q3 | `vmprinttest` | 7/7 pass; 4 dumps | `q3_vmprinttest.log` |
| Q4 | `lazytest`, `lazytest oom` | 46/46 pass | `q4_lazytest.log` |
| Q5 | `cowtest` | 14/14 pass | `q5_cowtest.log` |
| Q6 | `protecttest` | 27/27 pass | `q6_protecttest.log` |
| Q7 | `shmtest` | 14/14 pass | `q7_shmtest.log` |
| Q8 | `pagerepl`, `pagerepl 3 1 2 3 4 1 2 5` | table produced (A/3: 15/12/9) | `q8_pagerepl.log` |
| Final regression, first attempt | same commands as below | **hung** in `usertests` → `linkunlink`; pre-existing file-system deadlock, see below | `regression_attempt1_usertests_hang.log` |
| Final regression (one boot) | `ls`, `echo`, `cat README \| wc`, `mkdir`, `forktest`, `usertests`, `threadtest`, `synctest`, all 8 Lab 8 programs, `memtest` again | everything passes; `memtest` baseline free pages are the same before and after the whole run | `regression.log` |

The regression boot exercises fork/wait/exec (shell, `forktest`, `usertests`), sbrk (`usertests` sbrktest), read/write (pipes, files) and the Lab 7 thread and semaphore code.

**Invalid accesses.** Every test that makes an invalid access does it in a child, and the kernel prints the `page fault ... -- kill proc` line. No run produced a kernel panic.

## Intermittent `usertests` hang (pre-existing, not caused by Lab 8)

During the final full rerun, the regression boot hung inside `usertests` at the `linkunlink` test. That log is kept as `regression_attempt1_usertests_hang.log`. I investigated the hang before accepting any result.

1. **Where it hangs.** On timeout, the runner now sends Ctrl-P, which makes xv6 print `procdump`. In a reproduced Lab 8 hang, the two `usertests` processes were both asleep in `acquiresleep` inside `iput()`:
   - one called from `sys_link` → `dirlink`;
   - the other from `sys_open` → `create` → `dirlink` (resolved with `addr2line` on `kernel`).

   This is inode sleeplock code in `fs.c` and `sysfile.c`. `fs.c` is identical to Lab 7, and Lab 8 changed only the `argptr` lines of `sysfile.c`. That procdump log was overwritten by the next stress run; the stack above is from the console at the time.
2. **Same rate before Lab 8.** I ran the **unmodified baseline images** (built before any Lab 8 change) in 8 parallel boots: **2 of 8 hung**. One hung in `linkunlink` and one in `concreate`, with the same `sleep`/`acquiresleep`/`iput`/`dirlink` stacks (`baseline/baseline_usertests_hang_run4.log`, `..._run8.log`).
3. **Lab 8 totals.** Across all `usertests` boots of the Lab 8 kernel, 8 passed and 2 hung (1 of them in a 6-boot parallel stress run). The baseline totals are 7 passed and 2 hung. The rates are comparable.

**Conclusion.** This is a pre-existing intermittent file-system deadlock in the Lab 7 tree. It is triggered by concurrent link/unlink/create of the same name, and it is unrelated to the memory-management changes. It is not fixed here, because changing the file system is outside Lab 8. The final `regression.log` is a complete, passing rerun on a fresh `fs.img`.

`usertests` refuses to run twice on the same `fs.img` ("already ran user tests -- rebuild fs.img"). Use `rm fs.img; make fs.img` before rerunning it.

## Known test-visible behaviour changes

- `usertests` → `mem`: no `mem ok` line. Its child calls `malloc` until failure. With lazy `sbrk`, `malloc` keeps succeeding until physical memory runs out, and the child is then killed by the page-fault handler ("out of physical memory"). `usertests` continues and ends with ALL TESTS PASSED.
- `sbrktest` in `usertests` prints a `kill proc` line for each probe of kernel memory. That test expects those kills.

## Not tested (reviewed only)

- `kalloc()` failure while copying a COW page and inside `copyuvm()`: in both cases the process is killed or fork returns −1, with references dropped by `freevm`.
- Fork from a process whose other threads are running (eager-copy fallback). Lab 7 `threadtest` passes, but it does not measure the fallback.
- Multi-CPU TLB staleness after `mprotect`/`munprotect`/shrink in a multithreaded process (known limitation; xv6 has no TLB shootdown).
