# xv6 OS Lab Exam Execution & Command Reference

This document provides a complete quick-reference guide for building xv6, launching and exiting QEMU, and running the test programs for **every question in Lab 5 through Lab 10**.

---

## 1. Build and Boot Commands

All commands should be executed from the `lab_exam/xv6-public` directory on your Linux/WSL terminal:

```bash
cd lab_exam/xv6-public

# Clean build artifacts:
make clean

# Compile the kernel (using parallel jobs):
make -j4

# Build the filesystem image containing all lab programs:
make fs.img

# Launch xv6 in terminal mode (no graphical window needed):
make qemu-nox
```

> **How to Exit QEMU:**
> Press `Ctrl+A`, release both keys, and then press `X` (`Ctrl+A` then `x`).

---

## 2. Lab-by-Lab Execution Reference

Once xv6 finishes booting, you will be at the shell prompt (`$ `). Run each command as listed below.

---

### Lab 5 — Synchronization & Shared Memory

| Question | Program Command | Description | Expected Output / Pass Criteria |
|---|---|---|---|
| **Q1** | `peterson` | Peterson's Mutual Exclusion algorithm | Parent and child alternately enter critical sections; final counter value is exactly `20`. |
| **Q2** | `prodcons` | Producer–Consumer using semaphores | Producer produces items, consumer consumes items; buffer remains bounded; ends with `PRODUCER-CONSUMER TEST COMPLETED`. |
| **Q3** | `readwrite` | Readers–Writers problem | Multiple readers can read concurrently; writers get exclusive access; ends with `READERS-WRITERS TEST COMPLETED`. |
| **Q4** | `dining` | Dining Philosophers using semaphores | 5 philosophers each complete 5 eating cycles without deadlock; ends with `ALL PHILOSOPHERS COMPLETED THEIR CYCLES`. |

---

### Lab 6 — Deadlock & Resource Allocation

| Question | Program Command | Description | Expected Output / Pass Criteria |
|---|---|---|---|
| **Q1** | `bankers` | Banker's Safety & Resource Request Algorithm | Displays allocation matrices, finds safe sequence `<P1 -> P3 -> P4 -> P0 -> P2>`, approves safe requests, rejects unsafe requests. |
| **Q2** | `deadlockdetect` | Deadlock Detection via Wait-For Graph | Identifies allocation/request state, prints cycle: `DEADLOCK DETECTED! Deadlocked processes: P1 P2 P3`. |
| **Q3** | `resourceorder fixed` | Resource Ordering Deadlock Prevention | Processes acquire resources in ascending numerical order; prints `ALL 5 PROCESSES COMPLETED SUCCESSFULLY, No deadlock occurred`. |
| **Q4** | `syncdeadlock` | Synchronization & Deadlock Avoidance | Demonstrates resource hierarchy in dining philosophers; all philosophers eat and finish without deadlock. |

---

### Lab 7 — Threads, Semaphores & Priority Scheduling

| Question | Program Command | Description | Expected Output / Pass Criteria |
|---|---|---|---|
| **Q1** | `threadtest` | Kernel-Supported User Threads (`clone`, `join`) | Spawns threads, demonstrates data races without mutex, tests `join()`; prints `threadtest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q2** | `synctest` | Thread Synchronization with Semaphores | Multi-threaded producer-consumer and mutual exclusion; prints `synctest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q3** | `deadlock` | Deadlock Simulation & Detection Matrix | Simulates resource deadlock and tests recovery; prints `deadlock: ALL CHECKS PASSED (0 failure(s))`. |
| **Q4** | `priotest 400` | Priority Scheduler with Aging & Dispatch Fairness | Runs high, medium, and low priority tasks; verifies lower priority is aged up and scheduled; prints `priotest: ALL CHECKS PASSED (0 failure(s))`. |

---

### Lab 8 — Memory Management & Virtual Memory

| Question | Program Command | Description | Expected Output / Pass Criteria |
|---|---|---|---|
| **Q1** | `vatest` | Virtual to Physical Translation (`va2pa`) | Translates global, local, malloc, unmapped, and COW pages; prints `vatest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q2** | `meminfotest` | Memory Statistics & Accounting (`getmeminfo`) | Measures total, free, proc, pt, resident, and shared pages; tests for leaks; prints `memtest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q3** | `vmprinttest` | Hierarchical Page Table Dump (`vmprint`) | Dumps page directory and PTEs through sbrk and deallocation; prints `vmprinttest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q4** | `lazytest` | Lazy Heap Allocation (`sbrk`, `T_PGFLT`) | Tests 32 MB lazy sbrk, sparse touch, out-of-bounds kills; prints `lazytest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q5** | `cowtest` | Copy-On-Write Fork (`PTE_COW`, refcounts) | Verifies shared physical frames across `fork()`, COW on write, refcount drops, leak cycles; prints `cowtest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q6** | `protecttest` | Memory Protection (`mprotect`, `munprotect`) | Verifies read-only page protection, traps on illegal writes, inheritance across fork; prints `protecttest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q7** | `shmtest` | Keyed Shared Memory (`shm_open`, `shm_close`) | Tests multi-process keyed shared memory communication and cleanup; prints `shmtest: ALL CHECKS PASSED (0 failure(s))`. |
| **Q8** | `pagerepl` | Page Replacement Simulation | Simulates FIFO and LRU on 20 reference strings across 3, 4, 5 frames; displays comparative hit/fault statistics. |

---

### Lab 9 — Page Tables & Demand Paging

| Question | Program Command | Description | Expected Output / Pass Criteria |
|---|---|---|---|
| **Q1** | `pttest` | Process Page Table Walk & Dump (`vmprint`) | Inspects PTEs before sbrk, after lazy sbrk, and after touching pages; prints `pttest: [PASS] exactly 4 new pages appeared, pttest: ALL CHECKS PASSED`. |
| **Q2** | `faulttest` | Demand Paging & Lazy Fault Tracking (`getfaults`) | Verifies lazy page fault counts, fork isolation, and illegal access termination; prints `faulttest: ALL CHECKS PASSED`. |
| **Q3** | `pagerepl` | FIFO and LRU Replacement Simulation | Runs page replacement simulation and displays table of faults and hit ratios. |

---

### Lab 10 — Physical Memory Tracking & Address Translation

| Question | Program Command | Description | Expected Output / Pass Criteria |
|---|---|---|---|
| **Q1** | `memtest` | Physical Memory Tracking (`memstats`) | Allocates 16 pages, verifies allocation count increases by 16, frees pages, verifies count resets; prints `Page count increased by 16 [PASS]`. |
| **Q2** | `vttest` | Virtual-to-Physical Address Translation (`vtop`) | Translates stack and heap addresses, verifies page offset preservation and frame sharing; prints `vttest complete`. |
| **Q3** | `pagetest` | Page-Table Inspection & Protection (`pageinfo`, `setreadonly`) | Displays frame and flags, calls `setreadonly()`, forks child to write; child is terminated by page fault trap 14; prints `Child was terminated by page fault [PASS]`. |
| **Q4** | `pagereplace` | FIFO, LRU, and Optimal Simulation | Simulates FIFO, LRU, and Optimal algorithms across 3, 4, 5 frames; displays complete comparison table (FIFO=15/10/9, LRU=12/8/7, OPT=9/8/7). |

---

## 3. Automated Batch Verification Script

To run all automated test suites sequentially without typing each command manually, run the provided Python runner from the host shell:

```bash
# Run Lab 5 synchronization suite:
python3 runxv6.py log_lab5.log peterson prodcons readwrite dining

# Run Lab 6 deadlock & Banker's suite:
python3 runxv6.py log_lab6.log bankers deadlockdetect "resourceorder fixed" syncdeadlock

# Run Lab 7 threads & scheduler suite:
python3 runxv6.py log_lab7.log threadtest synctest deadlock "priotest 400"

# Run Lab 8 virtual memory & COW suite:
python3 runxv6.py log_lab8.log vatest meminfotest vmprinttest lazytest cowtest protecttest shmtest

# Run Lab 9 & Lab 10 page table & memory suite:
python3 runxv6.py log_lab9_10.log pttest faulttest pagerepl memtest vttest pagetest pagereplace
```
