# xv6 OS Lab Comprehensive Question Index (Lab 5 – Lab 10)

This index provides a complete, structured mapping of every question from Lab 5 through Lab 10 implemented and integrated into the unified `xv6-public` codebase.

---

## Summary Matrix

| Lab | Question | Question Name | Program | Kernel Files Modified | User Files Created / Modified | Execution Command | Compiled & Tested |
|---|---|---|---|---|---|---|---|
| **Lab 5** | Q1 | Peterson's Mutual Exclusion Algorithm | `peterson` | `proc.c`, `proc.h`, `vm.c`, `memlayout.h`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `peterson.c`, `user.h`, `usys.S` | `peterson` | Yes (PASS) |
| **Lab 5** | Q2 | Producer–Consumer Problem (Semaphores) | `prodcons` | `semaphore.c`, `defs.h`, `sysproc.c`, `syscall.c`, `syscall.h` | `prodcons.c`, `user.h`, `usys.S` | `prodcons` | Yes (PASS) |
| **Lab 5** | Q3 | Readers–Writers Problem (Semaphores) | `readwrite` | `semaphore.c`, `defs.h`, `sysproc.c`, `syscall.c`, `syscall.h` | `readwrite.c`, `user.h`, `usys.S` | `readwrite` | Yes (PASS) |
| **Lab 5** | Q4 | Dining Philosophers Problem (Semaphores) | `dining` | `semaphore.c`, `defs.h`, `sysproc.c`, `syscall.c`, `syscall.h` | `dining.c`, `user.h`, `usys.S` | `dining` | Yes (PASS) |
| **Lab 6** | Q1 | Banker's Algorithm (Safety & Allocation) | `bankers` | None (uses standard system & user library) | `bankers.c`, `Makefile` | `bankers` | Yes (PASS) |
| **Lab 6** | Q2 | Deadlock Detection via Wait-For Graph | `deadlockdetect` | None (uses standard system & user library) | `deadlockdetect.c`, `Makefile` | `deadlockdetect` | Yes (PASS) |
| **Lab 6** | Q3 | Resource Ordering Deadlock Prevention | `resourceorder` | `semaphore.c` (semaphores) | `resourceorder.c`, `Makefile` | `resourceorder fixed` (or `resourceorder bad`) | Yes (PASS) |
| **Lab 6** | Q4 | Combined Synchronization & Deadlock Avoidance | `syncdeadlock` | `semaphore.c` (semaphores) | `syncdeadlock.c`, `Makefile` | `syncdeadlock` | Yes (PASS) |
| **Lab 7** | Q1 | Kernel-Supported User Threads (`clone`, `join`) | `threadtest` | `proc.c`, `proc.h`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `threadtest.c`, `user.h`, `usys.S` | `threadtest` | Yes (PASS) |
| **Lab 7** | Q2 | Thread Synchronization with Semaphores | `synctest` | `semaphore.c`, `sysproc.c`, `syscall.c`, `syscall.h` | `synctest.c`, `user.h`, `usys.S` | `synctest` | Yes (PASS) |
| **Lab 7** | Q3 | Deadlock Simulation & Real Circular Wait | `deadlock` | `pipe.c`, `proc.c` | `deadlock.c`, `user.h`, `usys.S` | `deadlock` | Yes (PASS) |
| **Lab 7** | Q4 | Priority Scheduler with Aging & Dispatch Fairness | `priotest` | `proc.c`, `proc.h`, `trap.c`, `sysproc.c`, `syscall.c`, `syscall.h` | `priotest.c`, `user.h`, `usys.S` | `priotest 400` (or `priotest`) | Yes (PASS) |
| **Lab 8** | Q1 | Virtual to Physical Address Translation (`va2pa`) | `vatest` | `vm.c`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `vatest.c`, `user.h`, `usys.S` | `vatest` | Yes (PASS) |
| **Lab 8** | Q2 | Physical Memory Statistics (`getmeminfo`) | `meminfotest` | `kalloc.c`, `vm.c`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `meminfo.h`, `meminfotest.c`, `user.h`, `usys.S` | `meminfotest` | Yes (PASS) |
| **Lab 8** | Q3 | Hierarchical Page-Table Dump (`vmprint`) | `vmprinttest` | `vm.c`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `vmprinttest.c`, `user.h`, `usys.S` | `vmprinttest` | Yes (PASS) |
| **Lab 8** | Q4 | Lazy Heap Memory Allocation (`sbrk`, `T_PGFLT`) | `lazytest` | `proc.c`, `sysproc.c`, `trap.c`, `vm.c` | `lazytest.c`, `user.h`, `usys.S` | `lazytest` | Yes (PASS) |
| **Lab 8** | Q5 | Copy-On-Write Fork (`PTE_COW`, page refcounts) | `cowtest` | `kalloc.c`, `vm.c`, `proc.c`, `trap.c`, `mmu.h`, `defs.h` | `cowtest.c`, `user.h`, `usys.S` | `cowtest` | Yes (PASS) |
| **Lab 8** | Q6 | Page Protection (`mprotect`, `munprotect`) | `protecttest` | `vm.c`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `protecttest.c`, `user.h`, `usys.S` | `protecttest` | Yes (PASS) |
| **Lab 8** | Q7 | Keyed Shared Memory Regions (`shm_open`, `shm_close`) | `shmtest` | `proc.c`, `proc.h`, `vm.c`, `memlayout.h`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `shmtest.c`, `user.h`, `usys.S` | `shmtest` | Yes (PASS) |
| **Lab 8** | Q8 | Page Replacement Simulation (FIFO, LRU, Optimal) | `pagerepl` | User space simulation | `pagerepl.c`, `user.h` | `pagerepl` | Yes (PASS) |
| **Lab 9** | Q1 | Process Page Table Walk & Dump (`vmprint`) | `pttest` | `vm.c`, `sysproc.c`, `syscall.c`, `syscall.h`, `defs.h` | `pttest.c`, `user.h`, `usys.S` | `pttest` | Yes (PASS) |
| **Lab 9** | Q2 | Demand Paging & Lazy Fault Tracking (`getfaults`) | `faulttest` | `proc.c`, `sysproc.c`, `trap.c`, `vm.c`, `syscall.c`, `syscall.h`, `defs.h` | `faulttest.c`, `user.h`, `usys.S` | `faulttest` | Yes (PASS) |
| **Lab 9** | Q3 | FIFO and LRU Replacement Simulation | `pagerepl` | User space simulation | `pagerepl.c`, `user.h` | `pagerepl` | Yes (PASS) |
| **Lab 10** | Q1 | Physical Memory Tracking (`memstats`) | `memtest` | `kalloc.c`, `defs.h`, `sysproc.c`, `syscall.c`, `syscall.h` | `memtest.c`, `user.h`, `usys.S` | `memtest` | Yes (PASS) |
| **Lab 10** | Q2 | Virtual-to-Physical Address Translation (`vtop`) | `vttest` | `vm.c`, `defs.h`, `sysproc.c`, `syscall.c`, `syscall.h` | `vttest.c`, `user.h`, `usys.S` | `vttest` | Yes (PASS) |
| **Lab 10** | Q3 | Page-Table Flags & Read-Only Protection (`pageinfo`, `setreadonly`) | `pagetest` | `vm.c`, `defs.h`, `sysproc.c`, `syscall.c`, `syscall.h` | `pagetest.c`, `user.h`, `usys.S` | `pagetest` | Yes (PASS) |
| **Lab 10** | Q4 | FIFO, LRU, and Optimal Replacement Analysis | `pagereplace` | User space simulation | `pagereplace.c`, `user.h` | `pagereplace` | Yes (PASS) |
