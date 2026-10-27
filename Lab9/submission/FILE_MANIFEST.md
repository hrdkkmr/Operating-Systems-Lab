# File manifest: Lab 9

All paths are relative to `Lab9/`. The work started from the Lab 7 xv6 tree (`Lab7/xv6`), the same base Lab 8 used. "Changed lines" counts the added and removed lines in `diff Lab7/xv6/F Lab9/xv6/F`. Every kernel change carries a `Lab 9` comment.

## Modified kernel and build files (`xv6/`)

| File | Changed lines | Change |
|---|---:|---|
| `vm.c` | 129 | `lazylock`; `lazyalloc()` (Q2); `vmprint()` and `printhex8()` (Q1); `copyuvm()` skips pages that were never allocated; comment in `deallocuvm()`; NULL check in `uva2ka()`; `copyout()` allocates lazy pages |
| `trap.c` | 48 | `T_PGFLT` case and `lazyfault()`: allocate below `sz`, otherwise print pid/address and kill |
| `proc.h` | 1 | `int pgfaults` in `struct proc` |
| `proc.c` | 28 | `growproc_lazy()`; `pgfaults = 0` in `allocproc()` |
| `exec.c` | 4 | resets `pgfaults` for the new program image |
| `sysproc.c` | 23 | lazy `sys_sbrk`; `sys_vmprint`, `sys_getfaults` |
| `syscall.h` | 2 | `SYS_vmprint` 30, `SYS_getfaults` 31 |
| `syscall.c` | 4 | externs and table entries |
| `defs.h` | 3 | `growproc_lazy`, `lazyalloc`, `vmprint` |
| `user.h` | 2 | `vmprint()`, `getfaults()` |
| `usys.S` | 3 | stubs |
| `Makefile` | 3 | `_pttest`, `_faulttest`, `_pagerepl` in `UPROGS` |

## New files (`xv6/`)

| File | Purpose |
|---|---|
| `pttest.c` | Q1 test |
| `faulttest.c` | Q2 test |
| `pagerepl.c` | Q3 FIFO/LRU simulation |
| `runxv6.py` | test driver copied unchanged from Lab 8: boots QEMU, types the commands, saves the log and screen captures, always kills QEMU |
| `runlab9.sh` | runs every test (one boot per question) plus a regression boot |

## Submission (`submission/`)

| Path | Content |
|---|---|
| `REPORT.md`, `README.md`, `TESTING.md`, `FINAL_STATUS.md`, `FILE_MANIFEST.md`, `SCREENSHOT_CHECKLIST.md` | documentation |
| `output_logs/00_clean_build.log` | clean build output |
| `output_logs/q1_pttest.log`, `q2_faulttest.log`, `q3_pagerepl.log`, `regression.log` | full console output |
| `screenshots/<run>/NN_<command>.png` | QEMU VGA screen after each command |

## Other

- `PROGRESS.md`: the checklist I worked from.
- `submission.zip`: an archive of the xv6 sources (no build products) and of `submission/`.
