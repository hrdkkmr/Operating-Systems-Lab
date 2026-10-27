# Testing: Lab 9

## Environment

- Windows 11 with the WSL distro `Ubuntu`: gcc 15.2.0, GNU make, `qemu-system-i386`, python3.
- QEMU runs as the Makefile does (`-smp 2 -m 512`, `fs.img` + `xv6.img`). It uses the serial console; the VGA screen is captured through the QEMU monitor.

## Reproducing

From `Lab9/xv6` in Linux/WSL:

```sh
make clean
make fs.img xv6.img          # build the kernel and all user programs
sh runlab9.sh                # all tests -> ../submission/output_logs, ../submission/screenshots
```

To run the tests by hand:

```sh
make qemu-nox
$ pttest
$ faulttest
$ pagerepl
$ pagerepl 3 1 2 3 4 1 2 5
```

To leave QEMU, press `Ctrl-a x`. `usertests` will not run twice on the same `fs.img`; run `rm fs.img; make fs.img` before running it again.

## Results

| Step | Command(s) | Result | Log |
|---|---|---|---|
| Clean build | `make clean; make fs.img xv6.img` | success; Lab 9 code adds no warnings (only the existing `sh.c`, `mp.c` and linker RWX warnings) | `00_clean_build.log` |
| Q1 | `pttest` | 3 → 3 → 7 valid pages; ALL CHECKS PASSED | `q1_pttest.log` |
| Q2 | `faulttest` | 0 faults at start, 5 after 5 pages; fork OK; 3 invalid accesses kill only the child; ALL CHECKS PASSED | `q2_faulttest.log` |
| Q3 | `pagerepl`, `pagerepl 3 1 2 3 4 1 2 5`, `pagerepl 4 1 2 3 4 1 2 5` | FIFO 15 / LRU 12 with 3 frames; table for 3–5 frames | `q3_pagerepl.log` |
| Regression (one boot) | `ls`, `echo`, `cat README \| wc`, `mkdir`, `forktest`, `usertests`, `threadtest`, `synctest`, `pttest`, `faulttest`, `pagerepl` | everything passes (`usertests`: ALL TESTS PASSED) | `regression.log` |

No run produced a kernel panic.

## Expected kill messages in `regression.log`

- **40 × `usertests: invalid page fault ... err 0x5`:** `sbrktest` probes kernel addresses from user mode and expects each probe to be killed.
- **1 × `usertests: out of memory for lazy page`:** the `mem` test's child calls `malloc` until memory runs out. With lazy `sbrk`, `malloc` keeps succeeding, and the child is killed by the fault handler when `kalloc` fails. `usertests` continues normally.
- **1 × `trap 13`:** the `uio` test runs a privileged I/O instruction on purpose. The Lab 7 and Lab 8 baselines print the same line.
