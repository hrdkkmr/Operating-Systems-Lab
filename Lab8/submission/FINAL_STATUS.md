# Final status — Lab 8 (Memory Management in xv6)

Status labels:

- **Implemented and tested successfully**
- **Implemented but not tested**
- **Partially implemented**
- **Not implemented**
- **Requires clarification because the specification is missing**

"Tested" means the program was run in QEMU and its output is saved in `output_logs/`. Every log ends with `ALL CHECKS PASSED` (or, for `pagerepl`, the measured table).

## Specification source

`OS_Lab_Assignment-8.pdf` gives detailed tasks only for **Q1–Q4**. Its "Common Submission Requirements (for all 8 questions)" also name `cowtest.c`, `protecttest.c`, `shmtest.c` and `pagerepl.c`. They ask for free-page figures for Q5 and a FIFO/LRU/Optimal table for Q8.

I searched for the Q5–Q8 text in all of these places and found nothing:

- the repository (all labs);
- the README files;
- git history (`git log --all`);
- all branches (only `master` and `origin/master` exist).

Q5–Q8 below are therefore **inferred extensions**, designed from the program names. Their interfaces (`mprotect`/`munprotect`, `shm_open`/`shm_close`, the `pagerepl` command line) are my own choices.

## Q1 — Virtual to physical address translation (verified requirement)

| Requirement | Status |
|---|---|
| a) `int va2pa(int va)` using `walkpgdir()` (exported in `defs.h`); −1 if not present or no `PTE_U` | Implemented and tested successfully |
| b) kernel `cprintf` of PDX, PTX, offset, P/W/U | Implemented and tested successfully |
| c) `vatest`: global, local, malloc VA→PA; 0x7FFFFFFF → −1 | Implemented and tested successfully |
| d) fork: same VA, different PA | Implemented and tested successfully. With Q5's COW, the PAs differ after the first write; immediately after fork they are shared (both stages shown and explained) |

## Q2 — Physical memory statistics (verified requirement)

| Requirement | Status |
|---|---|
| a) free-page counter updated in `kalloc`/`kfree` under `kmem.lock` | Implemented and tested successfully |
| b) `getmeminfo()`: total_pages, free_pages, sz/PGSIZE, page-table pages | Implemented and tested successfully (plus extra fields: user page tables, resident pages, shared pages) |
| c) `memtest`: stats, grow, stats, fork | Implemented and tested successfully |
| d) free pages return to the earlier value after the child exits and is reaped | Implemented and tested successfully (also a 20-cycle leak test and a whole-regression-run comparison) |

## Q3 — Page-table dump (verified requirement)

| Requirement | Status |
|---|---|
| a) `vmprint()`: valid PDEs with index, page-table PA, flags | Implemented and tested successfully |
| b) indented PTEs (index, VA, PA, P/W/U), only below KERNBASE, one-line kernel summary | Implemented and tested successfully |
| c) `vmprinttest`: start-up, after `sbrk(8*4096)`, after `sbrk(-4096)` | Implemented and tested successfully. An extra dump after touching the pages is needed because `sbrk` is lazy (Q4) |
| d) guard page identified; user pages = sz/PGSIZE − guard | Implemented and tested successfully |

## Q4 — Lazy allocation (verified requirement)

| Requirement | Status |
|---|---|
| a) `sys_sbrk` only increases `sz` for n > 0; shrinking frees real pages | Implemented and tested successfully |
| b) `T_PGFLT`: `rcr2()`, heap check (below sz, above the guard/stack), `kalloc` + zero + `mappages(PTE_W\|PTE_U)`; otherwise message + kill | Implemented and tested successfully |
| c) `kalloc` failure kills | Implemented and tested successfully (`lazytest oom`) |
| c) faults ≥ sz, below the stack/heap, guard page | Implemented and tested successfully |
| c) kernel touching lazy memory (`read`/`write` with an untouched buffer) | Implemented and tested successfully |
| c) `copyuvm()` skips absent PTEs instead of panicking | Implemented and tested successfully |
| d) `lazytest`: 32 MB `sbrk`, a few touched pages, `getmeminfo` before and after, fork, exit, untouched-buffer syscall | Implemented and tested successfully |

## Q5 — Copy-on-write fork (inferred extension)

| Item | Status |
|---|---|
| Exact assignment wording | **Requires clarification because the specification is missing** |
| COW fork with refcounts, `PTE_COW`, write-fault copy, last-reference reuse, kernel-write handling, cleanup | Implemented and tested successfully (`cowtest`) |
| Eager-copy fallback when other threads share the address space | Implemented but not tested specifically (Lab 7 `threadtest` passes) |
| `kalloc` failure during a COW copy or inside `copyuvm` | Implemented but not tested (code-reviewed only) |

## Q6 — Memory protection (inferred extension)

| Item | Status |
|---|---|
| Exact assignment wording | **Requires clarification because the specification is missing** |
| `mprotect`/`munprotect` (`PTE_PROT`), fault isolation, guard and kernel pages, fork/COW interaction, `read()` into a protected page → −1 | Implemented and tested successfully (`protecttest`) |
| Concurrent `mprotect` by one thread while another runs on a different CPU (no TLB shootdown) | Known limitation, not tested |

## Q7 — Shared memory (inferred extension)

| Item | Status |
|---|---|
| Exact assignment wording | **Requires clarification because the specification is missing** |
| `shm_open`/`shm_close` keyed one-page regions; fork inheritance; exec/exit cleanup; lifetime via refcounts | Implemented and tested successfully (`shmtest`) |
| Access control between unrelated processes (any process may attach any key) | Not implemented (outside the inferred scope; documented) |

## Q8 — Page replacement (inferred extension)

| Item | Status |
|---|---|
| Exact assignment wording / required interface | **Requires clarification because the specification is missing** |
| FIFO, LRU, Optimal simulation on the same string and frame count, trace, fault table | Implemented and tested successfully (`pagerepl`) |
| Kernel-level replacement policy (swapping) | Not implemented. The PDF only asks for a comparison table, so I kept it as a user-space simulation |

## Common requirements

| Requirement | Status |
|---|---|
| a) modified kernel files, commented | Done; changed lines are marked `Lab 8` |
| b) eight user test programs | Done; all registered in `UPROGS` |
| c) terminal screenshots for each question | Done: real QEMU VGA `screendump` captures in `screenshots/` (last 25 screen lines; full output in `output_logs/`). See `SCREENSHOT_CHECKLIST.md` for optional manual window screenshots |
| d) 3–4 page report | `REPORT.md` |

## Regressions and open issues

- `usertests`: ALL TESTS PASSED both before and after the changes. The `mem` subtest no longer prints `mem ok`: under lazy allocation its child is killed with "out of physical memory" instead of `malloc()` returning 0. This is expected and documented.
- Lab 7 `threadtest` and `synctest`: ALL CHECKS PASSED both before and after.
- `usertests` occasionally hangs in `linkunlink`/`concreate`: an inode-sleeplock deadlock in `iput()` called from `dirlink()`. The **unmodified baseline** hangs at a comparable rate (2 of 8 parallel boots), so this is a pre-existing Lab 7 file-system issue, not caused by Lab 8. It is not fixed, because changing the file system is outside the Lab 8 scope. Details are in `TESTING.md`.
- **Open question for the instructor:** the intended interfaces for Q5–Q8. If the official text differs (for example, a different `mprotect` signature, System V-style `shmget`, or a kernel-level replacement policy), the corresponding part needs to be adapted.
