# Lab 9 progress checklist

Build and test environment: WSL distro `Ubuntu` (gcc 15.2, make, qemu-system-i386).
Source tree: `Lab9/xv6`, a copy of `Lab7/xv6` (the same base Lab 8 used).

- [x] Q1 `vmprint` syscall + `pttest`
- [x] Q2 lazy `sbrk`, `T_PGFLT` handler, `copyuvm`/`deallocuvm`/`uva2ka`/`copyout`, `pgfaults` + `getfaults` + `faulttest`
- [x] Q3 `pagerepl` (FIFO/LRU, trace, 3/4/5-frame table)
- [x] Regression: `usertests`, `forktest`, Lab 7 `threadtest`/`synctest`
- [x] `submission/` docs, logs, screenshots
- [x] `submission.zip`
