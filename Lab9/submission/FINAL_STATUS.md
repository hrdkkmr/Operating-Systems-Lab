# Final status: Lab 9

"Tested" means the program was run in QEMU and its output is saved in `output_logs/`. Every log for a test program ends with `ALL CHECKS PASSED`; for `pagerepl`, the log ends with the measured table instead.

## Q1: Printing the page table

| Requirement | Status |
|---|---|
| a) `int vmprint(void)` walks the page directory like `freevm()`; prints VA, PA and P/W/U for each valid user page; skips addresses ≥ KERNBASE | Implemented and tested successfully |
| b) prints the total number of valid user pages and of page-table pages | Implemented and tested successfully (the page directory is reported separately as "+1") |
| c) `pttest`: `vmprint` at start-up and after `sbrk(4*4096)`; exactly four new pages | Implemented and tested successfully. `sbrk` is lazy because of Q2, so `pttest` also prints a third dump after touching the pages; that dump shows exactly 4 new pages (3 → 7) |

## Q2: Lazy allocation

| Requirement | Status |
|---|---|
| a) `sys_sbrk` only increases `sz` for n > 0 | Implemented and tested successfully |
| a) `deallocuvm()` / `copyuvm()` skip pages that were never allocated | Implemented and tested successfully (fork with 27 untouched pages; `usertests` sbrktest) |
| b) `T_PGFLT`: `rcr2()`, below `sz` → `kalloc` + `memset` + `mappages(PTE_W\|PTE_U)`, counted in `proc.pgfaults`; otherwise print pid + address and kill | Implemented and tested successfully |
| c) `faulttest`: `sbrk(32*4096)`, 5 pages written, `getfaults()` = 5 | Implemented and tested successfully |
| c) an access above `sz` kills only that process | Implemented and tested successfully (also far above `sz`, and the guard page) |

**Additions beyond the spec, needed for correctness:**
- Lazy faults raised in kernel mode, when a system call touches an untouched buffer, are handled too.
- `copyout()` allocates lazy pages; `clone()` needs this.
- A lock stops two threads from mapping the same page twice.
- `exec()` resets the counter.

## Q3: FIFO and LRU

| Requirement | Status |
|---|---|
| a) `pagerepl` takes the frame count and a reference string; prints the frames and hit/fault for every reference | Implemented and tested successfully |
| b) the given string with 3 frames: FIFO 15, LRU 12 | Implemented and tested successfully (15 and 12) |
| c) table of faults and hit ratio for 3, 4 and 5 frames | Implemented and tested successfully |

## Common requirements

| Requirement | Status |
|---|---|
| a) modified kernel files, with each change commented | Done (marked `Lab 9`) |
| b) `pttest.c`, `faulttest.c`, `pagerepl.c` | Done |
| c) terminal screenshots for each question | Done: QEMU `screendump` captures in `screenshots/` |
| d) 1-page report | `REPORT.md` |

## Known limitations

- If `kalloc()` fails during a lazy fault **in kernel mode**, the kernel panics, because the faulting kernel instruction cannot be restarted safely. This was not triggered in any test. In user mode the process is simply killed; `usertests` `mem` exercises that path.
- `sbrk(-n)` by one thread while another thread of the same process faults on the shrinking region is not synchronised with `lazylock`. This is an edge case that was not tested.
- The shell accepts at most 10 arguments (`MAXARGS`), so a custom string for `pagerepl` can have at most 8 pages.
