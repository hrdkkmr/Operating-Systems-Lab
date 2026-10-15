# Operating Systems Lab 8 — Memory Management in xv6

**Platform:** traditional x86 xv6 (2-level paging, 4 KB pages). The code builds on my Lab 7 tree, which already has `clone`/`join` threads that share a page directory, the Lab 5 shared page at `SHMVA`, and a priority scheduler.

**Build/test environment:** WSL Ubuntu, gcc 15.2, `qemu-system-i386 -smp 2 -m 512`.

**Scope.** The assignment PDF specifies only Q1–Q4 in detail. For Q5–Q8 it gives only the program names (`cowtest`, `protecttest`, `shmtest`, `pagerepl`) and the report items (free-page figures for Q5, a FIFO/LRU/Optimal table for Q8). No other specification exists in the repository, its history or its branches. Q5–Q8 are therefore **inferred extensions**: I designed them from those names, and they are labelled that way throughout.

All numbers below are copied from `output_logs/`. Every test program ends with an `ALL CHECKS PASSED` line.

## Key data structures (summary)

| Structure | Where | Purpose |
|---|---|---|
| `kmem.nfree`, `kmem.ntotal`, `kmem.ref[PHYSTOP/PGSIZE]` | `kalloc.c` | free-page count (Q2), total pages, per-frame reference count (Q5/Q7) |
| `PTE_COW` (0x200), `PTE_PROT` (0x400) | `mmu.h` | software bits in the PTE's AVL field |
| `proc.heap_start` | `proc.h` | lowest address that may be allocated on demand (Q4) |
| `proc.shmmask`, `shmreg` table | `proc.h`, `proc.c` | attached shared regions (Q7) |
| `USERTOP = SHMREGBASE` | `memlayout.h` | heap limit, below the 8 shared-region pages and the Lab 5 page |
| `struct meminfo` | `meminfo.h` | `getmeminfo()` result, shared by kernel and user programs |

New system calls (numbers 30–36):

- `va2pa`
- `getmeminfo`
- `vmprint`
- `mprotect` and `munprotect`
- `shm_open` and `shm_close`

## Q1 — Virtual to physical address translation

**Design.**
- `walkpgdir()` is no longer `static` and is declared in `defs.h`.
- `sys_va2pa` reads the argument as a full 32-bit word and calls `va2pa(pgdir, va)` in `vm.c`. That function prints PDX (`va>>22`), PTX (`(va>>12)&0x3FF`), the offset (`va&0xFFF`), the PDE, the PTE and its P/W/U bits.
- It only dereferences the page table when the PDE is present.
- It returns `PTE_ADDR(pte) | offset`, or −1 when the page is not present or `PTE_U` is clear.
- Physical memory ends at 224 MB, so a valid result is always positive and cannot be confused with −1.

**Results** (`q1_vatest.log`):

| Object | VA | PDX/PTX/offset | PA |
|---|---|---|---|
| global | 0x14A4 | 0 / 1 / 0x4a4 | 0xDEE74A4 |
| local (stack) | 0x3FA8 | 0 / 3 / 0xfa8 | 0xDEE5FA8 |
| malloc block | 0xBF98 | 0 / 11 / 0xf98 | 0xDFBDF98 |
| 0x7FFFFFFF | — | 511 / 1023 / 0xfff | −1 (PDE not present) |
| 0x80100000 (kernel) | — | 512 / 256 / 0 | −1 (P=1, U=0) |

**Fork.** The PDF expects different physical addresses because xv6 copies pages on fork. Since Q5 makes fork copy-on-write, the test shows two stages:

1. Immediately after `fork()`, the child's PTE is `0xdee7265` (`PTE_COW` set, W=0). Child and parent map the **same** frame, 0xDEE74A4.
2. After the child writes `global_var`, its address becomes **0xDEE24A4**, while the parent stays at 0xDEE74A4. The values are now 100 and 200.

Same VA, different PA after the first write: this is the PDF's expected outcome, reached through COW.

The test also shows lazy allocation: an untouched `sbrk` page returns −1 until it is written.

## Q2 — Physical memory statistics

**Allocator.**
- `kalloc()` decrements `nfree` only when a page actually leaves the free list, so a failed allocation changes nothing.
- `kfree()` increments it only when a frame truly returns to the list.
- `freerange()` counts `ntotal` and seeds each frame with reference 1 before freeing it, so initialisation goes through the same code path.
- All counters change under `kmem.lock`.
- `kfree()` of a frame whose reference count is already 0 panics with `kfree: page already free`, which catches double frees.

**`getmeminfo`.**
- The destination is validated with `argptrw()` *before* counting, so a page allocated for the result buffer itself is already included.
- The result is written with `copyout()`.
- `proc_pages = sz/PGSIZE`, exactly as the PDF specifies.
- `pt_pages` counts the page directory plus every present PDE. xv6's `setupkvm()` gives every process its *own* copy of the kernel page tables, so `pt_pages` is 66 for a small process: 1 directory + 1 user page table + 64 kernel page tables.
- Extra fields: `pt_user_pages`, `resident_pages` (present `PTE_U` pages) and `shared_pages` (refcount > 1).
- `proc_pages` (logical size) and `resident_pages` (physically backed) differ under lazy allocation and COW. This is the reason the extra fields exist.

**Measurements** (`q2_memtest.log`; total_pages = 57035):

| Step | free_pages | Change |
|---|---:|---|
| baseline | 56755 | — |
| `sbrk(64 pages)` + touch every page | 56691 | −64 (no new page table needed) |
| child alive, right after fork (child's view) | 56622 | fork = −69 |
| child wrote its 64 heap pages | 56558 | −64 (COW copies) |
| child exited and reaped | **56691** | back to the pre-fork value |
| `sbrk(−64 pages)` | **56755** | back to the baseline |
| 20 × (sbrk, touch, fork, child writes, exit, wait, shrink) | 56755 → **56755** | no leak |

In the regression boot (`regression.log`), `memtest` measures **56755** free pages at baseline both before and after `usertests` and all Lab 8 programs ran. No pages leaked across the whole run.

## Q3 — Page-table dump

**Design.**
- `vmprint()` walks PDEs 0–511 (va < KERNBASE).
- For each present PDE it prints the virtual range, the page table's physical address and its P/W/U flags.
- It reaches the table through `P2V(PTE_ADDR(pde))` and never follows a non-present PDE.
- It then prints each present PTE: VA, frame address and flags. Present PTEs below `sz` without `PTE_U` are annotated as the guard page; COW, mprotect and shared-memory pages are also annotated.
- Summary line, by definition: the number of valid PDEs at index ≥ 512 and the number of pages they map. Here that is **64 PDEs mapping 65536 pages**: 56 PDEs for KERNBASE..KERNBASE+PHYSTOP plus 8 for DEVSPACE.

Start-up dump (`q3_vmprinttest.log`):

```
 PDE 0: va 0x0-0x3fffff  page table pa 0xdee8000  [P W U]
     PTE 0: va 0x0 -> pa 0xdee9000  [P W U]
     PTE 1: va 0x1000 -> pa 0xdee7000  [P W U]
     PTE 2: va 0x2000 -> pa 0xdee6000  [P W -]  <- stack guard page (PTE_U clear)
     PTE 3: va 0x3000 -> pa 0xdee5000  [P W U]
user: 3 present pages with PTE_U, 1 guard page(s); sz/PGSIZE = 4, sz/PGSIZE - guard = 3
```

**Guard page.** `exec()` allocates two pages above the program and calls `clearpteu()` on the lower one. The guard page is therefore *present* (it has a frame and P=1) but has no `PTE_U`, so a stack overflow causes a protection fault instead of silently writing into data. It still counts in `sz`, which is why the user-accessible count is `sz/PGSIZE − 1`.

**Heap growth and shrink.** Because `sbrk` is lazy (Q4), the PDF's "new entries appear after `sbrk()`" holds only once the pages are touched. The test therefore dumps four times:

| Dump | When | sz pages | PTE_U pages |
|---|---|---:|---:|
| 1 | start-up | 4 | 3 |
| 2 | right after `sbrk(8*4096)` | 12 | 3 (lazy) |
| 3 | after touching the 8 pages | 12 | 11 (PTEs 4–11 appear) |
| 4 | after `sbrk(−4096)` | 11 | 10 (PTE 11 removed) |

## Q4 — Lazy allocation

**`sbrk`.**
- For `n > 0`, `sys_sbrk` calls `sbrklazy()`, which only increases `sz`. It updates `sz` for every thread that shares the page table, under `ptable.lock`. It rejects wrap-around and anything above `USERTOP`.
- For `n < 0`, the call still goes through `growproc()`/`deallocuvm()`. These free only present pages and skip absent PTEs and whole missing page tables.
- I added a check that rejects shrinking below address 0.

**Fault handler.** `trap.c` handles `T_PGFLT` by reading `rcr2()` and `tf->err` and calling `pagefault()`, which runs `uvmfault()` under `ptable.lock`.

- If the page is absent and `heap_start ≤ va < sz`:
  1. `kalloc()` a frame;
  2. `memset` it to zero;
  3. `mappages(..., PTE_W|PTE_U)`.

  `heap_start` is set by `exec()` to the top of the stack, so text, data, the guard page and the stack are never treated as lazy.
- Every other case fails, and the faulting user process is killed with a diagnostic message:
  - address ≥ `sz`;
  - address below the heap;
  - a page without `PTE_U`;
  - a kernel address;
  - `kalloc` failure (the frame is released if the page-table allocation fails).

**Kernel access to user memory.** The kernel reads and writes user memory directly, so I made every access path resolve pages first:

- `fetchint`, `fetchstr` (checked page by page) and `argptr` call `uservalid()`. It allocates untouched heap pages and refuses pages without `PTE_U`. As a side effect, `read(fd, guard_page, n)` now fails instead of writing the guard page.
- `copyout()` resolves pages itself: it handles `exec()` and `clone()`, whose thread stacks are normally lazy `malloc` memory.
- `uva2ka()` no longer dereferences a NULL PTE.
- `copyuvm()` skips absent PTEs and whole missing page tables instead of calling `panic()`.
- As a last-resort safety net, a kernel-mode fault on a user address is resolved the same way.

**Measurements** (`q4_lazytest.log`):

| Step | free_pages | sz pages | Resident |
|---|---:|---:|---:|
| initial | 56757 | 4 | 3 |
| `sbrk(32 MB)` | **56757** (unchanged) | 8196 | 3 |
| touch 4 pages at 0, 8 MB, 16 MB+20 KB, 32 MB−4 KB | 56750 | 8196 | 7 |
| `sbrk(−32 MB)` | 56752 | 4 | 3 |

- Touching cost exactly 4 data frames plus 3 new page tables.
- After the shrink, every data frame was freed. The remaining 5 frames are user page tables that xv6's `deallocuvm()` keeps until exit.
- `read()` into an untouched buffer and `write()` from one both work.
- `fork()` with mostly-absent pages works, and free pages return to the pre-fork value after `wait()`.
- Each invalid access killed only its child, with a reason:
  - read at `sz`+4 KB;
  - write at 0x70000000;
  - read of 0x80000000;
  - write to the guard page.
- `lazytest oom`: a child `sbrk`s 240 MB and touches every page. It was killed at va 0xfd2b000 ("out of physical memory"), and every frame came back. 32 MB fits easily: 57035 pages ≈ 223 MB.

## Q5 — Copy-on-write fork (inferred extension)

**Design.**
- `copyuvm(pgdir, sz, cow)` maps the parent's frame into the child for every `PTE_U` page. For writable or mprotected pages it clears `PTE_W` and sets `PTE_COW` in both PTEs, then calls `kincref()` on the frame and flushes the parent's TLB.
- The guard page is still copied eagerly.
- A write fault on a `PTE_COW` page copies the frame if its reference count is > 1. Otherwise it simply re-enables `PTE_W`, because this process holds the last reference.
- Every existing `kfree()` call now drops one reference, so exit, shrink, exec and failed-fork cleanup needed no special cases.
- CR0.WP is set, so kernel writes to user memory also go through these checks. `read()`, `fstat()`, `pipe()` and `getmeminfo()` use `argptrw()`, which breaks COW before the kernel writes.
- With Lab 7 threads, other CPUs may hold writable TLB entries for pages that `copyuvm` makes read-only, and xv6 has no TLB shootdown. A fork while other threads share the address space therefore falls back to the original eager copy.

**Measurements** (`q5_cowtest.log`; 32 heap pages, 35 resident user pages):

| Event | free_pages |
|---|---:|
| parent before fork | 56723 |
| child right after fork | 56654 (−69 = 66 page-table pages + 1 kernel stack + 2 stack pages written since; **no heap page copied**) |
| child's first write | −1 page (new frame 0xDEC4000; the parent keeps 0xDFBF000) |
| child writes the other 31 pages | −31 |
| child reaped | **56723** |
| 30 fork/write/exit cycles | 56723 → **56723**, 0 shared pages left |

- After the child exited, the parent's write reused its frame without copying (reference count 1).
- `read()` into a COW buffer in the child gave the child a private frame and left the parent's data intact.

## Q6 — Memory protection (inferred extension)

**Design.**
- `mprotect(addr, len)` and `munprotect(addr, len)` take a page-aligned address and a length in pages; the range must lie inside `sz`.
- `mprotect` clears `PTE_W` and sets `PTE_PROT`, under `ptable.lock`. Absent lazy pages in the range are allocated first, and guard or kernel pages are rejected.
- A write fault on a `PTE_PROT` page kills the process, and `argptrw()` makes `read()` into such a page return −1.
- `PTE_PROT` survives fork and COW. `munprotect` re-enables `PTE_W` unless the page is still COW-shared; in that case the next write produces a private, writable copy.

**Results** (`q6_protecttest.log`; all 27 checks pass):

- bad arguments rejected;
- a protected page stays readable, and a write kills only the writer;
- `read()` into it returns −1, while `write()` from it works;
- a child inherits the protection, and after `munprotect` the child gets its own copy while the parent stays protected;
- a write to the program's own text page after `mprotect` kills the process (xv6 normally maps text writable);
- reading or writing the guard page, and writing kernel memory, kill the process.

## Q7 — Shared memory (inferred extension)

**Design.**
- `shm_open(key)` attaches one of 8 one-page regions, creating it zero-filled on first use, at `SHMREGBASE + key*4096`. The address range 0x7FFF7000–0x7FFFEFFF lies above `USERTOP`, so the heap can never grow into it.
- `shm_close(key)` detaches a region.
- The registry holds one reference on the frame and every mapping holds another, so the frame cannot be freed while it is still mapped.
- `fork()` children inherit the mapping of the same frame; it is not COW.
- `exec()` and `wait()` (for a zombie) detach all regions before `freevm()`.
- As with the Lab 5 page, the calls are refused while threads share the address space.

**Results** (`q7_shmtest.log`):

- The parent writes, and the child (same pa 0xDF2C000) reads "hello from the parent" and replies.
- A shared counter goes from 1 to 42.
- A program started with `exec()` finds the region by its key and writes back.
- A region stays valid in a child after the parent detaches.
- Free pages: 56756 at start, 56754 with region 3 attached, 56755 after the last detach. The frame is freed; the 1-page difference from the start is the retained page table for that 4 MB slot.

## Q8 — Page replacement: FIFO, LRU, Optimal (inferred extension)

`pagerepl` is a user-space simulator; the kernel's allocator is unchanged.

- All three algorithms always get the same reference string and frame count.
- FIFO evicts the page loaded earliest.
- LRU evicts the page whose last use is oldest.
- Optimal scans the rest of the actual reference string and evicts the page used farthest in the future (or never again; ties go to the lowest frame).
- The program prints a step-by-step trace with each eviction.

Measured (`q8_pagerepl.log`):

| String | Frames | Reference string | FIFO | LRU | Optimal |
|---|---:|---|---:|---:|---:|
| A | 3 | 7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1 | 15 | 12 | 9 |
| A | 4 | same | 10 | 8 | 8 |
| B | 3 | 1 2 3 4 1 2 5 1 2 3 4 5 | 9 | 10 | 7 |
| B | 4 | same | 10 | 8 | 6 |

**Analysis.**
- Optimal is a lower bound, but it needs future knowledge that a real kernel does not have, so it serves only as a benchmark.
- On string A, LRU (12) is closer to Optimal (9) than FIFO (15) is: the string has temporal locality, which LRU exploits. FIFO evicts by arrival time, even when a page is in heavy use (it evicts 0 at step 6 and reloads it at step 7).
- With 4 frames on A, LRU and Optimal tie at 8. Exact ties like this are a property of the string, and I report the result as measured.
- String B shows **Belady's anomaly** for FIFO: 9 faults with 3 frames but 10 with 4. LRU and Optimal are stack algorithms and cannot show the anomaly (10 → 8 and 7 → 6).
- On B with 3 frames, LRU (10) is worse than FIFO (9): this string has little locality.

## Problems and solutions

1. **The kernel dereferences user pointers directly** in `fetchint`, `argptr` and others. Under lazy allocation, those addresses may be absent. I added `uservalid()` for page-by-page resolution and a kernel-mode fault safety net.
2. **`copyout` writes through the kernel mapping**, bypassing `PTE_W`. That would have written into a COW-shared frame and broken isolation. `copyout` now resolves the page as a write fault, and write-intent buffers use `argptrw()`.
3. **`uva2ka` dereferenced a NULL PTE** when the page table was missing, which happens routinely with lazy heaps. I added the missing check.
4. **Lab 7 threads share a page directory and xv6 has no TLB shootdown.** Faults are resolved under `ptable.lock`, and a fork from a multithreaded process falls back to an eager copy.
5. **The Lab 5 shared page and the new regions lie above the heap.** `USERTOP` caps `sz` below them, and they are unmapped before `freevm()`.
6. **My first `cowtest` criterion was wrong.** It expected fork to cost fewer than 32 pages, but each process needs 66 page-table pages. The check now subtracts the measured page-table pages and the kernel stack.
7. **`usertests`' `mem` test.** It calls `malloc` until failure, but a lazy `sbrk` never fails before memory is exhausted. Its child is now killed with "out of physical memory" instead of seeing `malloc` return 0. `usertests` still reports ALL TESTS PASSED, but the `mem ok` line no longer appears. This is the expected trade-off of lazy allocation.
8. **The xv6 shell accepts fewer than 10 words per command**, which limits custom `pagerepl` input to 7 references.
9. **One regression boot hung in `usertests`/`linkunlink`.** A Ctrl-P process dump showed two processes waiting on inode sleeplocks in `iput()`, called from `dirlink()`: pure file-system code, with `fs.c` unchanged from Lab 7. The unmodified baseline hung in 2 of 8 parallel boots with the same stacks, so this is a pre-existing intermittent deadlock, not a Lab 8 regression. A rerun on a fresh `fs.img` passed (see `TESTING.md`).

## Testing and limitations

**Testing.**
- `xv6/runlab8.sh` boots a fresh QEMU per question, types the commands, and saves logs plus real VGA `screendump` captures. A hung test cannot block it, because the runner kills QEMU.
- Baseline before any change: `usertests` ALL TESTS PASSED, Lab 7 `threadtest`/`synctest` pass.
- After all changes, the same tests pass. All eight Lab 8 programs pass both individually and together in the regression boot, and free pages match before and after.

**Known limitations.**
- No TLB shootdown. `mprotect` and shrinking in a multithreaded process can leave stale entries on another CPU; this is the same documented limitation as Lab 7's shrinking.
- Page-table pages freed by a shrink stay allocated until exit, as in original xv6.
- Lazy `sbrk` can overcommit; the process is killed when it touches memory that does not exist.
- Untested error paths: `kalloc` failure *during a COW copy* or *inside `copyuvm`*, and the multi-CPU thread/TLB scenarios. These were reviewed but not exercised.

## Conclusion

Q1–Q4 are implemented as the PDF specifies and verified with measured free-page counts. Q5–Q8 are working, tested, inferred extensions; their exact original wording is unknown (see `FINAL_STATUS.md`).
