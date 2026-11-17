# xv6 OS Lab Implementation & Architecture Notes

This document provides deep architectural and engineering documentation for the integrated xv6 x86 operating system codebase spanning **Lab 5, Lab 6, Lab 7, Lab 8, Lab 9, and Lab 10**.

---

## 1. System-Call Harmonization & Numbering Map

Standard MIT xv6 reserves system calls 1 through 21. Across all six laboratory assignments, 20 additional system calls were implemented. To allow all 41 test and utility binaries to compile and execute without collisions, numbers 22 through 41 are assigned as follows:

| Syscall # | Name | Header Prototype (`user.h`) | Origin | Purpose |
|---|---|---|---|---|
| `22` | `clone` | `int clone(void(*fcn)(void*), void *arg, void *stack)` | Lab 7 Q1 | Create user thread sharing parent `pgdir` |
| `23` | `join` | `int join(void)` | Lab 7 Q1 | Wait for and reap a terminated child thread |
| `24` | `sem_init` | `int sem_init(int sem_id, int value)` | Lab 5/7 | Initialize kernel counting semaphore |
| `25` | `sem_wait` | `int sem_wait(int sem_id)` | Lab 5/7 | Decrement semaphore or sleep until > 0 |
| `26` | `sem_signal` | `int sem_signal(int sem_id)` | Lab 5/7 | Increment semaphore and wake waiting processes |
| `27` | `set_priority` | `int set_priority(int pid, int priority)` | Lab 7 Q4 | Set static process priority (1 to 20) |
| `28` | `get_priority` | `int get_priority(int pid)` | Lab 7 Q4 | Query priority of a live process |
| `29` | `shm_get` | `void* shm_get(void)` | Lab 5 Q1 | Map single shared memory page at `SHMVA` |
| `30` | `vmprint` | `int vmprint(void)` | Lab 9 Q1 | Dump process page table, return valid user pages |
| `31` | `getfaults` | `int getfaults(void)` | Lab 9 Q2 | Return number of lazy page faults handled |
| `32` | `memstats` | `int memstats(void)` | Lab 10 Q1 | Print allocation statistics, return current pages |
| `33` | `vtop` | `int vtop(int virtual_address)` | Lab 10 Q2 | Translate user virtual address to physical address |
| `34` | `pageinfo` | `int pageinfo(int virtual_address)` | Lab 10 Q3 | Display frame number and PTE flags (P, W, U) |
| `35` | `setreadonly` | `int setreadonly(int virtual_address)` | Lab 10 Q3 | Clear `PTE_W` flag from page table entry |
| `36` | `va2pa` | `int va2pa(int va)` | Lab 8 Q1 | Translate VA to PA with PDE/PTE diagnostics |
| `37` | `getmeminfo` | `int getmeminfo(struct meminfo *mi)` | Lab 8 Q2 | Populates memory and page table statistics struct |
| `38` | `mprotect` | `int mprotect(void *addr, int len)` | Lab 8 Q6 | Set `PTE_PROT` / clear `PTE_W` on range |
| `39` | `munprotect` | `int munprotect(void *addr, int len)` | Lab 8 Q6 | Clear `PTE_PROT` / restore `PTE_W` on range |
| `40` | `shm_open` | `void* shm_open(int key)` | Lab 8 Q7 | Attach/create keyed shared memory page |
| `41` | `shm_close` | `int shm_close(int key)` | Lab 8 Q7 | Detach keyed shared memory page |

---

## 2. Memory Layout & Shared Memory Architecture

xv6 x86 divides the 4 GB address space into User Memory (`0x00000000` to `0x80000000`) and Kernel Memory (`0x80000000` to `0xFFFFFFFF`, where `KERNBASE = 0x80000000`).

Both single-page shared memory (Lab 5) and keyed shared memory (Lab 8) sit safely at the very ceiling of user space, strictly below `KERNBASE` and above the user heap/stack:

```text
0x80000000  +-------------------------------------------------------+  KERNBASE
            | Lab 5 Single Shared Page (SHMVA = 0x7FFFF000)         |  4 KB
0x7FFFF000  +-------------------------------------------------------+
            | Lab 8 Keyed Region 7 (0x7FFFE000)                     |  4 KB
            | Lab 8 Keyed Region 6 (0x7FFFD000)                     |  4 KB
            | ...                                                   |  ...
            | Lab 8 Keyed Region 0 (0x7FFF7000)                     |  4 KB
0x7FFF7000  +-------------------------------------------------------+  USERTOP / SHMREGBASE
            |                                                       |
            |                    Free Address Space                 |
            |                                                       |
            +-------------------------------------------------------+
            | User Heap (grows upward via sbrk)                     |
            +-------------------------------------------------------+
            | User Stack (1 page, initially 0x3000 to 0x4000)       |
            +-------------------------------------------------------+
            | Stack Guard Page (PTE_P=1, PTE_U=0 at 0x2000)         |
            +-------------------------------------------------------+
            | Program Code & Data (loaded from ELF starting at 0x0) |
0x00000000  +-------------------------------------------------------+
```

### 2.1 Single-Page Shared Memory (Lab 5)
- **Virtual Address:** `SHMVA = KERNBASE - PGSIZE` (`0x7FFFF000`).
- **State Tracking:** `struct shmpage` protected by `shmpage.lock`. Tracks the shared physical frame `shmpage.pa` and the total attachment count `shmpage.refs`.
- **Inheritance:** Handled during `fork()`. If `curproc->shm_pa != 0`, `shm_map(np->pgdir, curproc->shm_pa)` maps the page into the child, `np->shm_pa = curproc->shm_pa`, and `shmpage.refs` is incremented.
- **Cleanup:** `shm_release(p)` unmaps the PTE and decrements `refs`. When `refs` drops to 0, `kfree()` frees the underlying frame.

### 2.2 Keyed Multi-Region Shared Memory (Lab 8)
- **Virtual Addresses:** `SHMREGBASE + key * PGSIZE`, for `key in 0..7` (`0x7FFF7000` to `0x7FFFE000`).
- **State Tracking:** `struct shmreg` protected by `shmreg.lock`. Tracks `shmreg.pa[8]` and `shmreg.attached[8]`.
- **Process State:** `p->shmmask` (bitmask of attached keys).
- **Reference Counting:** Uses `kincref(pa)` and `kfree(P2V(pa))`. When a region is first created, `kalloc()` allocates the frame and `shmreg.pa[key]` holds the registry reference. Each process mapping calls `uvmmapshared()`, which calls `kincref()`. Detaching drops the process reference; when `attached[key] == 0`, the registry drops its reference and zeroes `shmreg.pa[key]`.

---

## 3. Kernel Counting Semaphores

Implemented in `semaphore.c` and integrated into xv6:
- **Data Structure:** Array of 32 semaphores (`struct semaphore semaphores[MAX_SEMAPHORES]`), each containing a value, a spinlock, an active flag, and a channel pointer for sleep/wakeup.
- **Operations:**
  - `sem_init(id, value)`: Validates id and initial value; initializes the spinlock and sets `value`.
  - `sem_wait(id)`: Acquires the spinlock. In a `while(s->value <= 0)` loop, calls `sleep(s, &s->lock)`. Once woken and `value > 0`, decrements `s->value` and releases the lock.
  - `sem_signal(id)`: Acquires the spinlock, increments `s->value`, calls `wakeup(s)` to wake all waiting threads/processes, and releases the lock.
- **Deadlock & Lost-Wakeup Prevention:** The spinlock is held throughout the check and sleep handover, preventing lost wakeups between condition test and sleep state transition.

---

## 4. Kernel-Supported User Threads (`clone`, `join`)

- **Design:** Linux `clone(2)`-style light-weight processes.
- **Thread Creation (`clone`):**
  - Allocates a new `struct proc` via `allocproc()`.
  - Sets `np->pgdir = curproc->pgdir` (shares the exact same page directory).
  - Sets `np->sz = curproc->sz` and `np->heap_start = curproc->heap_start`.
  - Flags `np->is_thread = 1`.
  - Sets up the thread's private user stack with the passed `arg` and fake return address `0xFFFFFFFF`.
  - Sets `np->tf->eip = (uint)fcn` and `np->tf->esp = sp`.
  - Duplicates open file descriptors via `filedup()`.
- **Termination & Reaping:**
  - When a thread calls `exit()`, its state becomes `ZOMBIE`. It does NOT free `pgdir`.
  - `join()` searches for child threads of the calling leader, frees the thread's `kstack`, clears its process table slot, and returns its pid.
  - In `exec()` or `exit()` by the group leader: `reap_threads(curproc)` terminates all threads in the group and waits until all are reaped before the leader's `pgdir` is deallocated by `freevm()`.

---

## 5. Priority Scheduler with Aging

- **Process Fields:**
  - `p->priority`: Integer from 1 (highest priority) to 20 (lowest priority), default 10.
  - `p->wait_ticks`: Number of clock ticks spent in `RUNNABLE` state without being dispatched.
  - `p->last_sched`: Sequence number of the last time this process ran.
- **Dispatch Algorithm:**
  - Scans `ptable.proc` for `RUNNABLE` processes.
  - First selects the process with the highest effective priority (`min_priority`).
  - Among processes with the same priority, selects the one with the smallest `last_sched` (strict FIFO/round-robin fairness).
- **Aging Mechanism (`age_runnable`):**
  - Called from `trap.c` on timer interrupts (`IRQ_TIMER`) on CPU 0.
  - Iterates over all `RUNNABLE` processes.
  - Increments `wait_ticks`.
  - When `wait_ticks >= AGING_THRESHOLD` (30 ticks), the process priority is promoted by 1 (`p->priority--`, down to a floor of `MIN_PRIORITY = 1`), and `wait_ticks` resets to 0.
  - Starvation is completely eliminated.

---

## 6. Virtual Memory, Demand Paging, and Copy-On-Write

### 6.1 Demand Paging (Lazy Heap Allocation)
- When user code calls `sbrk(n)` with `n > 0`, `sbrklazy()` extends `curproc->sz` without allocating physical memory.
- When the process accesses an untouched heap page, the hardware MMU triggers trap 14 (`T_PGFLT`).
- The page-fault handler inspects the fault address in `rcr2()`. If `va >= heap_start && va < sz`, `uvmfault()` allocates a fresh physical frame, zero-initializes it (`memset(mem, 0, PGSIZE)`), maps it with `PTE_P | PTE_W | PTE_U`, and returns.
- Execution resumes transparently without process interruption.
- Absent pages in `[sz, USERTOP)` or writes to unmapped memory trigger diagnostic output and terminate the faulting process.

### 6.2 Copy-On-Write (COW) Fork
- In `fork()`, `copyuvm(curproc->pgdir, curproc->sz, cow=1)` iterates through all mapped user pages.
- For each present page with `PTE_U`:
  - If writable or protected (`PTE_W | PTE_PROT`), clears `PTE_W` and sets `PTE_COW`.
  - Maps the same physical frame into the child's page directory.
  - Increments the frame's reference count via `kincref(pa)`.
  - Flushes the parent's TLB via `flushtlb()`.
- When parent or child attempts to write to a COW page:
  - MMU raises `T_PGFLT` with write bit set in error code (`tf->err & 2`).
  - `uvmfault()` checks `*pte & PTE_COW`.
  - If reference count > 1: allocates a new frame, copies the 4096 bytes via `memmove()`, maps the new frame with `PTE_W` (clearing `PTE_COW`), and drops the old frame's refcount with `kfree()`.
  - If reference count == 1: the current process is the sole remaining owner; `uvmfault()` restores `PTE_W` and clears `PTE_COW` in place without allocating memory.

### 6.3 Page Protection (`mprotect` & `setreadonly`)
- **`mprotect(addr, len)` / `munprotect(addr, len)`:**
  - Validates that `addr` is page-aligned, `len > 0`, and the range lies within `p->sz`.
  - Allocates any untouched lazy pages in the range first so all pages are present.
  - Rejects pages without `PTE_U` (such as the stack guard page at `0x2000`).
  - Clears `PTE_W` and sets `PTE_PROT` (software bit `0x400`).
  - Writes to protected pages trigger trap 14 with `FAULT_PROT`, terminating the process.
- **`setreadonly(va)` (Lab 10 Q3):**
  - Walks the page directory to find the PTE for `va`.
  - Clears `PTE_W` and invalidates the TLB via `lcr3()`.
  - Subsequent write attempts trigger page faults.
- **Kernel Buffer Safety (`argptrw`):**
  - Standard xv6 `argptr()` only verified that pointers were within `sz`.
  - When the kernel writes into user memory (e.g. `read(fd, user_buf, n)`), kernel mode with `CR0.WP` would panic if the user buffer was read-only.
  - `argptrw()` verifies write accessibility via `uservalid(..., write=1)`. If the target buffer is protected or unwritable, the system call safely fails with `-1` without panicking the kernel.

---

## 7. Memory Tracking & Statistics

- **Post-Boot Allocation Accounting:**
  - `pages_allocated`: Incremented on every `kalloc()` when `kmem.use_lock == 1`.
  - `pages_freed`: Incremented on every `kfree()` when `kmem.use_lock == 1`.
  - `kmem.nfree`: Number of free frames on the freelist.
  - `kmem.ntotal`: Total usable frames in the system (up to `PHYSTOP = 224 MB`).
- **`memstats()` (Lab 10 Q1):** Prints total allocated pages, total freed pages, current pages in use (`allocated - freed`), and available free pages.
- **`getmeminfo(mi)` (Lab 8 Q2):** Returns `total_pages`, `free_pages`, `proc_pages`, `pt_pages`, `pt_user_pages`, `resident_pages`, and `shared_pages` (frames with refcount > 1).

---

## 8. Page Replacement Simulations

Both Lab 9 (`pagerepl.c`) and Lab 10 (`pagereplace.c`) implement page replacement simulations:
- **Reference String:** 20 page references: `7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1`.
- **Algorithms:**
  - **FIFO:** Replaces the page that entered the frame set earliest.
  - **LRU:** Replaces the page whose last access was furthest in the past.
  - **Optimal (OPT):** Replaces the page that will not be used for the longest period in the future.
- **Analytical Results:**
  - Frame count = 3: FIFO = 15 faults, LRU = 12 faults, OPT = 9 faults.
  - Frame count = 4: FIFO = 10 faults, LRU = 8 faults, OPT = 8 faults.
  - Frame count = 5: FIFO = 9 faults, LRU = 7 faults, OPT = 7 faults.
- Observations: Optimal achieves the lowest fault rate; LRU consistently outperforms FIFO; no Belady's anomaly occurs on this reference string.
