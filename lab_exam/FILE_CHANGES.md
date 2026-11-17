# File Changes Summary: Integrated xv6 Codebase

This document describes every modified kernel file and newly created or added user/header file in `lab_exam/xv6-public`.

---

## 1. Kernel Files Modified

### `defs.h`
- **Why modified:** Exposes prototypes of kernel functions to other kernel modules and system call dispatchers.
- **Functionality added:**
  - Added prototypes for shared memory: `shm_get()`, `shm_release()`, `shm_map()`, `shm_unmap()`, `shm_map_reg()`, `shm_unmap_reg()`, `shm_open()`, `shm_close()`, `shmreg_release()`.
  - Added prototypes for kernel semaphores: `seminit()`, `sem_init()`, `sem_wait()`, `sem_signal()`.
  - Added prototypes for threads and scheduler: `clone()`, `join()`, `reap_threads()`, `set_priority()`, `get_priority()`, `age_runnable()`.
  - Added prototypes for memory diagnostics: `kmemstats()` (Lab 10), `kmemstat()` (Lab 8), `uvmstat()` (Lab 8).
  - Added prototypes for address translation and page protection: `vtop_translate()`, `pageinfo_query()`, `setreadonly_vm()`, `va2pa()`, `uvmprotect()`.
  - Added prototype for page table dump: `vmprint()`.
  - Added prototype for demand paging: `lazyalloc()`.
- **Lab questions depending on it:** Lab 5 (Q1–Q4), Lab 6 (Q3–Q4), Lab 7 (Q1–Q4), Lab 8 (Q1–Q4, Q6, Q7), Lab 9 (Q1, Q2), Lab 10 (Q1–Q3).

### `proc.h`
- **Why modified:** Extends `struct proc` with state required for threading, priority scheduling, shared memory, and page-fault accounting.
- **Functionality added:**
  - `uint shm_pa`: Stores the physical address of the single shared memory page at `SHMVA`.
  - `uint shmmask`: Bitmask of attached keyed shared memory regions (0..7).
  - `int is_thread`: Distinguishes threads (created via `clone()`) from normal processes; prevents destroying shared address space upon thread termination.
  - `int priority`: Scheduling priority (1 to 20, default 10).
  - `int wait_ticks`: Ticks spent runnable without scheduling; drives dynamic aging.
  - `uint last_sched`: Sequence counter to ensure FIFO/round-robin ordering among equal-priority processes.
  - `int pgfaults`: Counts handled lazy page faults for the calling process.
- **Lab questions depending on it:** Lab 5 (Q1), Lab 7 (Q1, Q4), Lab 8 (Q7), Lab 9 (Q2).

### `proc.c`
- **Why modified:** Implements process lifecycle, thread support, priority scheduling with aging, and shared memory management across `fork()`, `exit()`, and `wait()`.
- **Functionality added:**
  - `pinit()`: Initializes locks for process table, shared page (`shmpage`), keyed regions (`shmreg`), and semaphores.
  - `allocproc()`: Zeroes thread and shared memory fields, sets default priority (10), and resets `pgfaults`.
  - `fork()`: Handles address space cloning, copies shared page reference (`shmpage.refs++`), and inherits attached keyed regions (`shmreg.attached[]++`).
  - `clone()`: Creates kernel threads sharing `pgdir` with separate user stacks and kernel stacks.
  - `join()`: Reaps child threads without freeing shared page directory.
  - `reap_threads()`: Ensures threads are terminated and reaped before a group leader's `pgdir` is freed.
  - `scheduler()`: Implements priority-based scheduling with round-robin for equal priorities and aging mechanism.
  - `age_runnable()`: Periodically decrements priority (increments effective priority) of starved runnable processes.
  - `shm_get()`, `shm_release()`: Allocates and releases the single shared memory page mapped at `SHMVA`.
  - `shm_open()`, `shm_close()`, `shmreg_release()`: Manages keyed shared memory regions at `SHMREGBASE`.
- **Lab questions depending on it:** Lab 5 (Q1), Lab 7 (Q1, Q4), Lab 8 (Q7).

### `kalloc.c`
- **Why modified:** Tracks physical memory page allocations, frees, provides statistics to user programs, and manages per-frame reference counts for Copy-On-Write and shared memory.
- **Functionality added:**
  - Counters `pages_allocated`, `pages_freed`, and `ntotal_pages`.
  - Array `ushort ref[PHYSTOP / PGSIZE]` for physical frame reference counting.
  - `kincref(pa)`: Increments the reference count of physical page `pa`.
  - `kpageref(pa)`: Returns the reference count of physical page `pa`.
  - `kfree(v)`: Drops one reference; only returns the frame to the freelist when its refcount drops to 0.
  - `kalloc()`: Sets the initial frame reference count to 1.
  - `kmemstats()`: Outputs formatted statistics and returns current in-use frames for Lab 10 Q1 (`memstats()`).
  - `kmemstat()`: Populates total and free page counts for Lab 8 Q2 (`getmeminfo()`).
- **Lab questions depending on it:** Lab 8 (Q2, Q4, Q5, Q7), Lab 10 (Q1).

### `mmu.h`
- **Why modified:** Defines software page table entry (PTE) bits for copy-on-write and protection.
- **Functionality added:**
  - `#define PTE_COW 0x200`: Bit 9/11 indicates a shared copy-on-write page (`PTE_W` clear).
  - `#define PTE_PROT 0x400`: Bit 10 indicates a page made read-only via `mprotect()`.
- **Lab questions depending on it:** Lab 8 (Q5, Q6).

### `vm.c`
- **Why modified:** Manages page tables, virtual address translation, demand paging, Copy-On-Write (COW), TLB invalidation, and protection.
- **Functionality added:**
  - `flushtlb()`: Reloads `CR3` (`lcr3()`) to flush the local TLB when permissions change.
  - `shm_map()`, `shm_unmap()`: Maps and unmaps the shared page at `SHMVA` with user-write permissions.
  - `shm_map_reg()`, `shm_unmap_reg()` / `uvmmapshared()`, `uvmunmapshared()`: Maps and unmaps keyed shared regions at `SHMREGBASE` with proper reference counting.
  - `uvmfault()`: Unified page fault resolver for demand paging (lazy allocation), Copy-On-Write frame unsharing, and protection verification.
  - `lazyalloc()`: Demand paging helper for lazy allocation.
  - `copyuvm()`: Updated to tolerate unallocated (lazy) pages during `fork()`, and share writable pages Copy-On-Write (`PTE_COW`) with incremented frame refcounts.
  - `copyout()`: Resolves lazy and COW pages via `uvmfault()` before kernel writes to user space.
  - `uvmaccessok()`: Fast-path lock-free verification of buffer accessibility.
  - `vmprint()`: Walks and prints page directory entries and page table entries below `KERNBASE`.
  - `vtop_translate()`: Translates user virtual address to full physical address with offset for Lab 10 Q2.
  - `pageinfo_query()`: Inspects PTE and prints frame number and P, W, U flags for Lab 10 Q3.
  - `setreadonly_vm()`: Clears `PTE_W` flag from PTE and invalidates TLB with `lcr3()` for Lab 10 Q3.
  - `va2pa()`: Implements virtual-to-physical address translation with diagnostic prints for Lab 8 Q1.
  - `uvmstat()`: Gathers page table and resident/shared page counts for Lab 8 Q2.
  - `uvmprotect()`: Sets or clears `PTE_PROT` / `PTE_W` across arbitrary page ranges for Lab 8 Q6 (`mprotect`, `munprotect`).
- **Lab questions depending on it:** Lab 5 (Q1), Lab 8 (Q1–Q7), Lab 9 (Q1, Q2), Lab 10 (Q2, Q3).

### `sysfile.c`
- **Why modified:** Validates user-space write buffers before kernel file/pipe write operations to prevent kernel panics on protected or unmapped user pages.
- **Functionality added:**
  - Updated `sys_read()`, `sys_fstat()`, and `sys_pipe()` to use `argptrw()` instead of `argptr()`.
- **Lab questions depending on it:** Lab 8 (Q4, Q6).

### `trap.c`
- **Why modified:** Handles clock interrupt aging and page fault exceptions (`T_PGFLT`).
- **Functionality added:**
  - In `trap()`, under timer interrupt (`IRQ_0 + IRQ_TIMER`), calls `age_runnable()` every 10 ticks.
  - Under `T_PGFLT` (trap 14): retrieves faulting virtual address from `rcr2()`. If the fault is due to lazy allocation within process bounds (`va < myproc()->sz`), allocates the page via `lazyalloc()`, increments `myproc()->pgfaults`, and resumes execution. If out of bounds or a write to a protected/read-only page, prints fault diagnostic and terminates the faulting process.
- **Lab questions depending on it:** Lab 7 (Q4), Lab 8 (Q4, Q6), Lab 9 (Q2), Lab 10 (Q3).

### `semaphore.c`
- **Why modified:** Provides counting semaphores in the kernel.
- **Functionality added:**
  - Implements `seminit()`, `sem_init()`, `sem_wait()`, `sem_signal()` using an array of `struct semaphore` protected by spinlocks, utilizing xv6's `sleep()` and `wakeup()` primitives.
- **Lab questions depending on it:** Lab 5 (Q2, Q3, Q4), Lab 6 (Q3, Q4), Lab 7 (Q2).

### `memlayout.h`
- **Why modified:** Defines user virtual memory layout constants for shared memory.
- **Functionality added:**
  - `#define SHMVA (KERNBASE - PGSIZE)`: Fixed address for Lab 5 shared memory.
  - `#define NSHMREG 8`: 8 keyed shared regions for Lab 8.
  - `#define SHMREGBASE (KERNBASE - 9 * PGSIZE)`: Base address for Lab 8 keyed shared memory.
  - `#define USERTOP SHMREGBASE`: Defines the boundary between standard user space and shared regions.
- **Lab questions depending on it:** Lab 5 (Q1), Lab 8 (Q7).

### `param.h`
- **Why modified:** Increased filesystem capacity.
- **Functionality added:**
  - Increased `FSSIZE` to 4000 blocks to accommodate all 40 user test binaries on `fs.img`.
- **Lab questions depending on it:** All labs (UPROGS packaging).

### `syscall.h` & `syscall.c`
- **Why modified:** Allocates unique system call numbers and dispatches system calls to their kernel implementations.
- **Functionality added:**
  - Registered numbers 22 through 41 for `clone`, `join`, `sem_init`, `sem_wait`, `sem_signal`, `set_priority`, `get_priority`, `shm_get`, `vmprint`, `getfaults`, `memstats`, `vtop`, `pageinfo`, `setreadonly`, `va2pa`, `getmeminfo`, `mprotect`, `munprotect`, `shm_open`, `shm_close`.
- **Lab questions depending on it:** All labs.

### `sysproc.c`
- **Why modified:** Contains system call entry wrappers that parse arguments and call kernel functions.
- **Functionality added:**
  - Wrappers for `sys_clone`, `sys_join`, `sys_sem_init`, `sys_sem_wait`, `sys_sem_signal`, `sys_set_priority`, `sys_get_priority`, `sys_shm_get`, `sys_vmprint`, `sys_getfaults`, `sys_memstats`, `sys_vtop`, `sys_pageinfo`, `sys_setreadonly`, `sys_va2pa`, `sys_getmeminfo`, `sys_mprotect`, `sys_munprotect`, `sys_shm_open`, `sys_shm_close`.
- **Lab questions depending on it:** All labs.

### `exec.c`
- **Why modified:** Handles address space cleanup during program image replacement.
- **Functionality added:**
  - Reaps thread group members before destroying old image.
  - Detaches Lab 5 shared page (`shm_release()`) and Lab 8 keyed regions (`shmreg_release()`).
  - Resets `curproc->pgfaults = 0`.
- **Lab questions depending on it:** Lab 5 (Q1), Lab 7 (Q1), Lab 8 (Q7), Lab 9 (Q2).

### `Makefile`
- **Why modified:** Registers new object files, user programs, compiler flags, and filesystem targets.
- **Functionality added:**
  - Added `semaphore.o` to kernel `OBJS`.
  - Added `-fno-pie -no-pie` and warning suppression flags to `CFLAGS` for modern GCC compatibility.
  - Added all 25 lab user executables to `UPROGS` and `clean` target.
- **Lab questions depending on it:** All labs.

---

## 2. Header and User Files Added

### `meminfo.h`
- Added header defining `struct meminfo` with fields `total_pages`, `free_pages`, `proc_pages`, `pt_pages`, `pt_user_pages`, `resident_pages`, and `shared_pages`. Used by Lab 8 user tests.

### User Test Programs Added
- **Lab 5:**
  - `peterson.c`: Tests Peterson's mutual exclusion algorithm.
  - `prodcons.c`: Tests Producer–Consumer with semaphores.
  - `readwrite.c`: Tests Readers–Writers with semaphores.
  - `dining.c`: Tests Dining Philosophers with semaphores.
- **Lab 6:**
  - `bankers.c`: Banker's safety check and resource requests.
  - `deadlockdetect.c`: Deadlock detection with wait-for graphs.
  - `resourceorder.c`: Resource ordering demonstration (bad vs fixed).
  - `syncdeadlock.c`: 5-process 3-resource synchronization test.
- **Lab 7:**
  - `threadtest.c`: Tests kernel threads and race conditions.
  - `synctest.c`: Tests synchronized threads and bounded buffer.
  - `deadlock.c`: Deadlock detection matrix & real-process pipe deadlock.
  - `priotest.c`: Priority scheduler verification and aging trace.
- **Lab 8:**
  - `vatest.c`: Tests `va2pa` translation.
  - `meminfotest.c`: Tests `getmeminfo` memory accounting and leak checking.
  - `vmprinttest.c`: Tests `vmprint` page table dump across heap growth/shrink.
  - `lazytest.c`: Tests lazy allocation behavior and error handling.
  - `protecttest.c`: Tests `mprotect` and `munprotect`.
  - `shmtest.c`: Tests keyed shared memory open, inheritance, and cleanup.
- **Lab 9:**
  - `pttest.c`: Tests page table dump.
  - `faulttest.c`: Tests lazy page faults and `getfaults`.
  - `pagerepl.c`: FIFO and LRU page replacement simulation.
- **Lab 10:**
  - `memtest.c`: Tests `memstats` physical page tracking.
  - `vttest.c`: Tests `vtop` virtual-to-physical translation.
  - `pagetest.c`: Tests `pageinfo` flags and `setreadonly` protection fault.
  - `pagereplace.c`: FIFO, LRU, and Optimal page replacement comparison.
