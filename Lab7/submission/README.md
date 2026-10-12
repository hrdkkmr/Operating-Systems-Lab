# OS Lab Assignment 7 — xv6 (x86): build, boot and run

The xv6 here is the MIT x86 xv6 (rev 11 layout: `proc.c`, `swtch.S`,
`trapasm.S`, `usys.S`, 32-bit `i386`), extended by Lab 5 (shared page and
semaphores) and by this lab. All code is in `../xv6/`; this folder holds
documentation, evidence and copies.

## Requirements

A Linux environment (WSL Ubuntu works) with `gcc` (multilib, `-m32`), `make`,
`perl` and `qemu-system-i386`. The results in this folder were produced with
gcc 15.2.0, QEMU 10.2.1 and SeaBIOS 1.17 under WSL2 (TCG emulation, no KVM).

## Build

```sh
cd Lab7/xv6
make clean
make            # kernel + xv6.img
make fs.img     # file system with all user programs
```

## Boot

```sh
make qemu-nox            # 2 CPUs (default), console in the terminal
make qemu-nox CPUS=1     # single CPU
make qemu-nox CPUS=4
```

Quit QEMU with `Ctrl-A` then `x`. `Ctrl-P` in the console prints the process
table, including each process's priority and thread flag.

**Important:** the Makefile passes `-smp N,sockets=N,cores=1,threads=1`. With
a plain `-smp N`, this QEMU/SeaBIOS combination advertises only one CPU in the
MP table and xv6 runs on a single CPU (the boot log then shows only
`cpu0: starting 0`). Check that the boot log lists `cpu1: starting 1` when
`CPUS >= 2`.

## Run the assignment programs (at the xv6 `$` prompt)

| Command | Question | Takes (2 CPUs, TCG) |
|---|---|---|
| `threadtest` | Q1: 3 threads × 100,000 unsynchronized increments, then 11 kernel checks | ~1 s |
| `threadtest 10` | repeat the experiment 10 times (shows the variation) | ~2 s |
| `threadtest 10 -n` | same, without the start barrier | ~2 s |
| `synctest` | Q2: unsynchronized vs semaphore counter, semaphore API checks, producer/consumer | ~75 s |
| `deadlock` | Q3: all matrix scenarios, then the real-process circular wait | ~4 s |
| `deadlock matrix` / `deadlock procs` | only one of the two parts | |
| `priotest` | Q4: API checks, priorities 5/10/15/20 with aging trace, equal-priority round robin | ~30 s (~80 s on 1 CPU) |
| `priotest 400` | the same with 1/10 of the work (aging barely shows) | ~2 s |

Every program ends with `ALL CHECKS PASSED` or `SOME CHECKS FAILED (n check
failures)`. Each `[PASS]`/`[FAIL]` line is computed at run time from what the
program observed.

The synchronized counter in `synctest` is slow by design: 600,000 contended
`sem_wait`/`sem_signal` system calls, each of which may sleep and be woken.

## Reproduce the logs and screenshots automatically

`tools/run_xv6.py` boots QEMU headless, types the commands, waits for each
prompt with a timeout (so nothing can hang the session), saves the raw
console to `--log`, and saves VGA screenshots with `--shots`:

```sh
cd Lab7/xv6 && make xv6.img fs.img
python3 ../submission/tools/run_xv6.py --xv6 . --cpus 2 \
    --log /tmp/q1.log --shots /tmp/shots "threadtest 10" "threadtest 10 -n"
```

`usertests` can run only once per `fs.img`. Run `rm fs.img && make fs.img`
before repeating it.

## Where to read more

* `REPORT.md`: design and results.
* `TESTING.md`: every test with its actual outcome.
* `FINAL_STATUS.md`: requirement checklist.
* `FILE_MANIFEST.md`: what changed and why.
