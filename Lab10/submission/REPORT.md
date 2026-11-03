# Operating Systems Lab 10: Memory Management, Virtual Memory, Paging and Page Replacement

**Base:** Lab 9 xv6 (x86) tree, which has `clone`/`join` threads, semaphores, priority scheduling, the Lab 5 shared page at `SHMVA`, lazy `sbrk`, and `vmprint`. **Environment:** WSL Ubuntu, gcc, `qemu-system-i386 -smp 2 -m 512`. Every new kernel change is marked with a `Lab 10` comment.

---

## Q1 — Physical Memory Tracking (`memstats`, `memtest`)

**Changes.**

- **`kalloc.c`:** two `static uint` counters (`pages_allocated`, `pages_freed`) are added to the `kmem` block.
  - `pages_allocated` is incremented inside `kalloc()` only when `kmem.use_lock == 1`, i.e., after `kinit2()` sets the flag. Boot-time page recycling is intentionally excluded.
  - `pages_freed` is incremented inside `kfree()` under the same condition, between the freelist-insert and the lock release, so the update is always atomic with respect to the allocator lock.
  - `freelist_count()` (static helper) counts the live freelist for the "available" figure without a second lock acquisition (already held by the caller).
  - `kmemstats(uint *allocated, uint *freed, uint *current, uint *avail)` acquires the lock, snapshots all four values, releases the lock, then prints a four-line table via `cprintf`.
- **`defs.h`:** adds `void kmemstats(uint*, uint*, uint*, uint*)`.
- **`sysproc.c`:** `sys_memstats()` calls `kmemstats()` and returns the current-in-use count as a convenient integer return value.
- Wired through `syscall.[ch]`, `usys.S`, `user.h` as syscall **32**.

**`memtest.c`** calls `memstats()` before allocation, allocates 16 pages with `sbrk(16*4096)`, touches every page (forcing lazy maps), calls `memstats()` again to confirm the count rose by 16, releases the memory with `sbrk(-65536)`, and calls `memstats()` one final time.

**Output** (`q1_memtest.log`). Before allocation: currently-in-use ≈ 9 (kernel + shell). After touching 16 pages: in-use increases by exactly 16. After `sbrk` shrink: in-use drops back. Available pages consistently sum with in-use to the total physical frames (≈ 3585 on a 512 MB QEMU machine).

---

## Q2 — Virtual-to-Physical Translation (`vtop`, `vttest`)

**Changes.**

- **`vm.c`:** `vtop_translate(pde_t *pgdir, uint va)` calls the existing `walkpgdir(pgdir, va, 0)` (no allocation). If the returned PTE pointer is NULL or `PTE_P` is clear, it prints a diagnostic via `cprintf` and returns -1. Otherwise it returns `PTE_ADDR(*pte) | (va & 0xFFF)` — the frame's base physical address OR'd with the 12-bit page offset — yielding the full physical address.
- **`defs.h`:** adds `int vtop_translate(pde_t*, uint)`.
- **`sysproc.c`:** `sys_vtop()` fetches one `argint`, passes it to `vtop_translate(myproc()->pgdir, ...)` and returns the result.
- Wired as syscall **33**.

**`vttest.c`** tests four cases: (1) the physical address of a stack variable, (2) the base of a freshly-touched heap page, (3) a mid-page offset inside the same heap page (confirms same frame, different offset), (4) a virtual address of 0x7fff0000 (well above `sz`), which must return -1.

**Output** (`q2_vttest.log`). Stack and heap cases return valid physical addresses. The mid-page address returns the same frame as the heap base. The out-of-range address returns -1 with the kernel printing "vtop: no page table entry for va 0x7fff0000".

---

## Q3 — Page Table Management and Protection (`pageinfo`, `setreadonly`, `pagetest`)

**Changes.**

- **`vm.c`:** two new functions:
  - `pageinfo_query(pde_t *pgdir, uint va)` walks the page table and prints the physical frame address, frame number, and the raw P/W/U bit values via `cprintf`. Returns 0 if present, -1 if not.
  - `setreadonly_vm(pde_t *pgdir, uint va)` clears `PTE_W` in the PTE and calls `lcr3(V2P(pgdir))` to flush the TLB immediately. Without the TLB flush the processor would continue using the cached writable mapping.
- **`defs.h`:** adds `int pageinfo_query(pde_t*, uint)` and `int setreadonly_vm(pde_t*, uint)`.
- **`sysproc.c`:** `sys_pageinfo()` and `sys_setreadonly()` each fetch one `argint` and delegate to the vm.c helpers, operating on `myproc()->pgdir`.
- Wired as syscalls **34** and **35**.

**`pagetest.c`** allocates one heap page, touches it, calls `pageinfo()` (shows `P=1 W=1 U=1`), calls `setreadonly()`, calls `pageinfo()` again (shows `P=1 W=0 U=1`), then forks a child. The child attempts `page[0] = 'Y'`, which faults. xv6 prints `pid X pagetest: trap 14 err 7 ...` and kills the child. The parent's `wait()` collects the child pid, confirming protection is enforced.

**Output** (`q3_pagetest.log`). The W flag transitions from 1 to 0. The child is killed by trap 14 (page fault, protection violation, error code 0x7 = user write to read-only page). The parent prints `[PASS]`.

---

## Q4 — Page Replacement Algorithms (`pagereplace`)

**Program.** `pagereplace.c` is a pure user-space simulation; no kernel changes are needed. It uses the assignment reference string `7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1` (20 references) and runs with 3, 4, and 5 frames.

- **FIFO** — a circular `frames[]` array with a `head` pointer tracks insertion order. On a fault the slot at `head` is replaced and `head` advances modulo `nframes`.
- **LRU** — a parallel `last_used[]` timestamp array records the logical time (reference index + 1) of every hit or load. On a fault the frame with the smallest timestamp is the victim.
- **Optimal** — for each frame the helper `next_use(page, pos)` scans `refs[]` forward from `pos` to find the next occurrence of that page, returning `NREFS` (infinity) if not found. The frame with the largest `next_use` value is evicted.

For every reference the program prints the current frame contents and either `HIT` or `FAULT`.

**Output** (`q4_pagereplace.log`), reference string of length 20:

| Algorithm | 3 frames | 4 frames | 5 frames |
|---|---|---|---|
| FIFO — faults | 15 | 10 | 9 |
| FIFO — hits | 5 | 10 | 11 |
| LRU — faults | 12 | 8 | 7 |
| LRU — hits | 8 | 12 | 13 |
| Optimal — faults | 9 | 8 | 7 |
| Optimal — hits | 11 | 12 | 13 |

- Optimal always achieves the minimum faults (theoretical lower bound).
- LRU closely tracks Optimal and is significantly better than FIFO for this string.
- All algorithms improve monotonically with more frames for this reference string (no Belady's anomaly observed for FIFO here).
- The gap between FIFO and Optimal with 3 frames (15 vs 9) illustrates how much a poor replacement choice costs when frames are scarce.

---

## Regression

On one boot, `usertests` (ALL TESTS PASSED), Lab 7 `threadtest` and `synctest`, `forktest`, `pttest`, `faulttest`, `pagerepl`, and standard shell commands all pass unchanged. The four new Lab 10 syscalls (32–35) do not affect any existing path.
