# Lab 10 — Memory Management, Virtual Memory, Paging, and Page Replacement

**Course:** Operating Systems  
**Assignment:** Lab 10  
**Base:** xv6 x86 (built incrementally on Lab 9)

---

## Objective

Implement four memory-related features in the xv6 operating system:

1. **Q1 — Physical Memory Tracking:** Modify the kernel allocator to count allocated and freed pages and expose the statistics via a new `memstats()` system call.
2. **Q2 — Virtual-to-Physical Translation:** Add a `vtop()` system call that walks the page table to translate any user virtual address into its physical address.
3. **Q3 — Page Table Inspection and Protection:** Add `pageinfo()` to display PTE flags and `setreadonly()` to remove write permission from a page.
4. **Q4 — Page Replacement Simulation:** Implement FIFO, LRU, and Optimal replacement algorithms in user space and compare their performance.

---

## Concepts Involved

- Physical memory allocator (`kalloc` / `kfree` free-list design)
- Two-level x86 page tables (page directory → page table → physical frame)
- Page-table entry (PTE) flags: Present (P), Writable (W), User (U)
- Virtual-address layout: PDX / PTX / offset
- TLB flushing via `lcr3()`
- FIFO, LRU, and Optimal page replacement algorithms
- Page faults triggered by protection violations

---

## Files Modified (Kernel)

| File | Change |
|---|---|
| `kalloc.c` | Added `pages_allocated` / `pages_freed` counters; added `kmemstats()` |
| `vm.c` | Added `vtop_translate()`, `pageinfo_query()`, `setreadonly_vm()` |
| `defs.h` | Added prototypes for `kmemstats`, `vtop_translate`, `pageinfo_query`, `setreadonly_vm` |
| `syscall.h` | Added `SYS_memstats=32`, `SYS_vtop=33`, `SYS_pageinfo=34`, `SYS_setreadonly=35` |
| `syscall.c` | Added extern declarations and dispatch-table entries for all four new syscalls |
| `sysproc.c` | Added `sys_memstats`, `sys_vtop`, `sys_pageinfo`, `sys_setreadonly` |
| `user.h` | Added user-space declarations for all four new syscalls |
| `usys.S` | Added `SYSCALL(memstats)`, `SYSCALL(vtop)`, `SYSCALL(pageinfo)`, `SYSCALL(setreadonly)` |
| `Makefile` | Added `_memtest`, `_vttest`, `_pagetest`, `_pagereplace` to UPROGS |

---

## Files Created (User Programs)

| File | Purpose |
|---|---|
| `memtest.c` | Q1: allocates pages via sbrk(), calls memstats() before/after/after-release |
| `vttest.c` | Q2: tests vtop() on stack, heap, mid-page, and unmapped addresses |
| `pagetest.c` | Q3: calls pageinfo(), setreadonly(), then forks a child to trigger a write fault |
| `pagereplace.c` | Q4: simulates FIFO, LRU, Optimal over the assignment reference string |

---

## Implementation Overview

### Q1 — `memstats()`

Two `static uint` counters (`pages_allocated`, `pages_freed`) are added to `kalloc.c`.
- `pages_allocated` is incremented inside `kalloc()` only when `kmem.use_lock == 1`
  (i.e., after `kinit2()` completes), so boot-time setup is excluded.
- `pages_freed` is incremented inside `kfree()` under the same condition.
- `kmemstats()` (callable from sysproc.c) reads both counters under the lock,
  computes `current = allocated - freed`, counts the freelist for `avail`, and
  prints a formatted summary via `cprintf()`.
- `sys_memstats()` calls `kmemstats()` and returns the current page count.

### Q2 — `vtop()`

`vtop_translate(pgdir, va)` in `vm.c` calls the existing (now reused) `walkpgdir()`
to obtain the PTE for `va` without allocating a new page table.
- If the PTE is missing or PTE_P is clear, returns -1.
- Otherwise returns `PTE_ADDR(*pte) | (va & 0xFFF)` — the frame base OR'd with the
  page offset — yielding the full physical address.

### Q3 — `pageinfo()` and `setreadonly()`

`pageinfo_query(pgdir, va)` walks the page table, prints the physical frame address,
frame number, and the P/W/U flag bits, then returns 0 or -1.

`setreadonly_vm(pgdir, va)` clears `PTE_W` in the PTE and immediately calls
`lcr3(V2P(pgdir))` to flush the TLB.  Any subsequent write to the protected page
triggers a `T_PGFLT` (trap 14) with error bit 1 set (protection violation); xv6
kills the faulting process.

### Q4 — `pagereplace.c`

Pure user-space simulation — no kernel changes required.

| Algorithm | Eviction policy |
|---|---|
| FIFO | Replace the page that has been in memory the longest (circular queue, `head` pointer) |
| LRU | Replace the page whose most-recent access was the earliest (timestamp array) |
| Optimal | Replace the page whose next use is furthest in the future (lookahead over `refs[]`) |

The program runs each algorithm with 3, 4, and 5 frames using the reference string:

```
7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1
```

For each frame count the per-reference state (frames + HIT/FAULT) is printed, followed by
a full comparison table.

---

## How to Compile and Run

```bash
cd Lab10/xv6
make clean
make
make qemu-nox    # or: make qemu
```

Inside xv6 shell:

```
$ memtest
$ vttest
$ pagetest
$ pagereplace
```

---

## Sample Outputs

### memtest
```
=== memtest: Physical Memory Allocation Tracking ===

[1] Statistics BEFORE allocation:
=== Physical Memory Statistics ===
  Total allocated pages : 847
  Total freed pages     : 838
  Currently in use      : 9
  Available free pages  : 3576
==================================
    (memstats returned current pages in use = 9)

[2] Allocated 16 pages (65536 bytes) via sbrk()
    Touched all 16 pages to force physical allocation.

[3] Statistics AFTER allocation:
=== Physical Memory Statistics ===
  Total allocated pages : 863
  Total freed pages     : 838
  Currently in use      : 25
  Available free pages  : 3560
==================================
    => Page count increased by 16  [PASS]
```

### vttest
```
=== vttest: Virtual-to-Physical Address Translation ===

[1] Stack variable address:
    VA = 0x2fb4
    PA = 0x3c2fb4  [PASS]

[2] Heap page (base of page, offset 0):
    VA = 0x3000
    PA = 0x3d0000  [PASS]
```

### pagetest
```
=== pagetest: Page Table Management and Page Protection ===

[2] Page info BEFORE setreadonly():
pageinfo: va 0x3000
  Physical frame address : 0x3d0000
  Physical frame number  : 976
  Flags: P=1 W=1 U=1

[4] Page info AFTER setreadonly():
pageinfo: va 0x3000
  Physical frame address : 0x3d0000
  Physical frame number  : 976
  Flags: P=1 W=0 U=1

[5] Forking child to write to the read-only page...
    Child (pid 4) was terminated by page fault  [PASS]
```

### pagereplace (excerpt, 3 frames)
```
--- FIFO  (frames = 3) ---
   7   [ 7 - - ]   FAULT
   0   [ 7 0 - ]   FAULT
   1   [ 7 0 1 ]   FAULT
   2   [ 2 0 1 ]   FAULT
   0   [ 2 0 1 ]   HIT
   ...
  Summary: faults=15, hits=5
```

---

## Testing Performed

| Test | Method | Result |
|---|---|---|
| Q1 compilation | `make` | Passes |
| Q1 counter increment | memtest sbrk + touch | Page count increases by 16 |
| Q1 counter decrement | memtest sbrk(-n) | Page count decreases |
| Q2 valid VA | vttest stack/heap | Correct PA returned |
| Q2 mid-page offset | vttest | Same frame, different offset |
| Q2 invalid VA | vttest 0x7fff0000 | Returns -1 |
| Q3 pageinfo flags | pagetest before setreadonly | P=1 W=1 U=1 |
| Q3 setreadonly | pagetest after call | W=0 confirmed |
| Q3 write fault | pagetest child fork | Child killed by trap 14 |
| Q4 FIFO 3 frames | pagereplace | 15 faults expected |
| Q4 LRU 3 frames | pagereplace | ≤15 faults |
| Q4 OPT 3 frames | pagereplace | Minimum faults |
| Regression | Lab 9 programs (pttest, faulttest, pagerepl) | All pass |

---

## Important Observations

- The `pages_allocated` counter is zeroed at system startup and only counts
  post-boot allocations (after `kinit2()` sets `use_lock = 1`).
- `vtop()` works for any mapped page; it returns -1 for lazy-allocated heap pages
  that have not yet been touched (their PTE_P bit is still 0).
- `setreadonly()` flushes the TLB with `lcr3()` immediately, so the protection
  takes effect for the very next instruction that accesses the page.
- The Optimal algorithm is not implementable in a real OS (it requires future
  knowledge), but serves as a lower bound for comparison.
- For the given reference string with 3 frames:
  FIFO = 15 faults (5 hits), LRU = 12 faults (8 hits), Optimal = 9 faults (11 hits).
