# Lab 8 progress checklist (resume point)

Build/test env: WSL distro `Ubuntu` (gcc 15.2, make, qemu-system-i386).
Source tree: `Lab8/xv6` (copy of Lab7/xv6: clone/join threads, Lab5 shm page at SHMVA, priority scheduler).
Spec: only Q1-Q4 detailed in PDF; no Q5-Q8 spec found in repo/history/branches.

- [x] Baseline build + boot (usertests ALL PASSED, threadtest/synctest pass; logs in submission/output_logs/baseline/)
- [x] kalloc: free/total counters + per-frame refcount
- [x] Q1 va2pa + vatest
- [x] Q2 getmeminfo + memtest
- [x] Q3 vmprint + vmprinttest
- [x] Q4 lazy sbrk, T_PGFLT, copyuvm/deallocuvm/copyout/argptr/fetch* + lazytest
- [x] Q5 COW (PTE_COW, refcount) + cowtest
- [x] Q6 mprotect/munprotect (PTE_PROT) + protecttest  [inferred]
- [x] Q7 shm_open/shm_close keyed regions + shmtest     [inferred]
- [x] Q8 pagerepl (user-space FIFO/LRU/OPT)              [inferred]
- [x] Regression: usertests, Lab7 tests
- [x] submission/ docs
- [x] final clean rebuild + full rerun; usertests hang investigated (pre-existing FS deadlock, baseline also hangs 2/8); regression rerun passed
- [x] submission.zip built
