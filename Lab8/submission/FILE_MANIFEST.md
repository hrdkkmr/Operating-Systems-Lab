# File manifest — Lab 8

All paths are relative to `Lab8/`. The starting point was the Lab 7 xv6 tree (`Lab7/xv6`); "changed lines" is the number of added and removed lines in `diff Lab7/xv6/F Lab8/xv6/F`. Every kernel change is marked with a `Lab 8` comment.

## Modified kernel and build files (`xv6/`)

| File | Changed lines | Change |
|---|---:|---|
| `kalloc.c` | 82 | Q2 free/total page counters; Q5/Q7 per-frame reference counts (`kincref`, `kpageref`), double-free check, `kmemstat` |
| `vm.c` | 368 | `walkpgdir` exported; `copyuvm` skips lazy pages and shares COW; `uvmfault` (demand allocation and COW); `va2pa`, `vmprint`, `uvmstat`, `uvmprotect`, shared-page map/unmap; `uva2ka` NULL check; `copyout` resolves lazy/COW pages; heap limit `USERTOP` |
| `proc.c` | 273 | `sbrklazy`, `pagefault`, `uservalid`, `mprotect`, `shm_open`/`shm_close`/`shmreg_*`; fork uses COW (eager when threads share the pgdir), inherits `heap_start` and shared regions; shrink checks in `growproc`; cleanup in `wait` |
| `proc.h` | 8 | `heap_start`, `shmmask` |
| `trap.c` | 37 | `T_PGFLT` handler |
| `exec.c` | 5 | sets `heap_start`; detaches shared regions |
| `syscall.c` | 38 | lazy/COW-aware `fetchint`, `fetchstr`, `argptr`; new `argptrw`; 7 new table entries |
| `syscall.h` | 8 | `SYS_va2pa` … `SYS_shm_close` (30–36) |
| `sysproc.c` | 106 | lazy `sys_sbrk`; `sys_va2pa`, `sys_getmeminfo`, `sys_vmprint`, `sys_mprotect`, `sys_munprotect`, `sys_shm_open`, `sys_shm_close` |
| `sysfile.c` | 9 | `read`, `fstat`, `pipe` use `argptrw` (kernel writes to user memory) |
| `defs.h` | 32 | prototypes, `FAULT_*` codes |
| `mmu.h` | 8 | `PTE_COW`, `PTE_PROT`, page-fault error bits |
| `memlayout.h` | 9 | `NSHMREG`, `SHMREGBASE`, `USERTOP` |
| `user.h` | 9 | user declarations of the 7 new system calls |
| `usys.S` | 7 | 7 system-call stubs |
| `Makefile` | 8 | 8 new programs in `UPROGS` |

## New files (`xv6/`)

| File | Purpose |
|---|---|
| `meminfo.h` | `struct meminfo` (shared by kernel and user code) |
| `vatest.c` | Q1 test |
| `memtest.c` | Q2 test |
| `vmprinttest.c` | Q3 test |
| `lazytest.c` | Q4 test |
| `cowtest.c` | Q5 test (inferred extension) |
| `protecttest.c` | Q6 test (inferred extension) |
| `shmtest.c` | Q7 test (inferred extension) |
| `pagerepl.c` | Q8 simulation (inferred extension) |
| `runxv6.py` | test driver: boots QEMU, types commands, saves the log and screen captures, always kills QEMU |
| `runlab8.sh` | runs every test, one boot per question, plus the regression boot |

## Submission (`submission/`)

| Path | Content |
|---|---|
| `REPORT.md`, `README.md`, `TESTING.md`, `FINAL_STATUS.md`, `FILE_MANIFEST.md`, `SCREENSHOT_CHECKLIST.md` | documentation |
| `output_logs/00_clean_build.log` | clean build output |
| `output_logs/baseline/` | runs of the unmodified tree (before any Lab 8 change), including the two `baseline_usertests_hang_run*.log` stress runs that show the pre-existing file-system deadlock |
| `output_logs/regression_attempt1_usertests_hang.log` | first final-regression attempt, which hit that same deadlock (kept as evidence) |
| `output_logs/q1_…q8_*.log`, `output_logs/regression.log` | full console output |
| `screenshots/<run>/NN_<command>.png` | QEMU VGA screen after each command |

## Other

- `PROGRESS.md`: working checklist used during development.
- `submission.zip`: archive of the xv6 sources (no build products) and of `submission/`.
- Unchanged: all other xv6 files and all other labs. The five `Screenshot 2026-09-08 *.png` files in `xv6/` are from an earlier lab and are not part of Lab 8.
