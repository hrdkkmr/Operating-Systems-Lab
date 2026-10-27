# Operating Systems Lab 9: Page Tables, Page Faults and Page Replacement

**Base:** my Lab 7 xv6 (x86) tree, which has `clone`/`join` threads sharing one page directory and the Lab 5 shared page at `SHMVA`. Lab 8 used the same base. **Environment:** WSL Ubuntu, gcc 15.2, `qemu-system-i386 -smp 2 -m 512`. Every kernel change is marked with a `Lab 9` comment. All numbers below come from `output_logs/`.

## Q1: Printing the page table (`vmprint`, `pttest`)

**Changes.**
- `vm.c: vmprint(struct proc*)` walks the page directory the same way `freevm()` does. It loops over the PDEs below `PDX(KERNBASE)`, so the kernel half is skipped, and over the 1024 PTEs of each present page-table page.
- For every present PTE it prints the VA (`PGADDR(pdx, ptx, 0)`), the frame's PA and the P/W/U flags. Pages without `PTE_U` are labelled as the stack guard page.
- At the end it prints the number of valid user pages and page-table pages, and it returns the page count.
- The new system call `vmprint()` (number 30) is wired through `sysproc.c`, `syscall.[ch]`, `usys.S` and `user.h`.

**Output** (`q1_pttest.log`). At start-up the process has 3 pages: text/data at 0x0, the guard page at 0x1000 (`P W -`) and the stack at 0x2000. It uses 1 page-table page plus the page directory.

| Dump | sz | valid user pages |
|---|---|---|
| start-up | 0x3000 | 3 |
| after `sbrk(4*4096)` | 0x7000 | 3 |
| after touching the 4 pages | 0x7000 | 7 |

Because Q2 makes `sbrk` lazy, the four pages do not appear right after `sbrk`; only `sz` grows. After one write to each page, exactly four new PTEs (0x3000–0x6000, `P W U`) appear, each with its own frame.

## Q2: Lazy allocation (`faulttest`, `getfaults`)

**Changes.**
- **`sysproc.c`:** when n > 0, `sys_sbrk` calls the new `growproc_lazy()` in `proc.c`. That function only increases `sz`, and does so for every thread sharing the page directory. It refuses overflow and growth into `SHMVA`. When n < 0, the old `growproc()` still frees real pages.
- **`trap.c`, `T_PGFLT`:**
  - **Lazy fault:** the handler reads `rcr2()`. If the address is below `myproc()->sz` and the error code says "not present", it calls `lazyalloc()`. That function (`kalloc` + `memset` + `mappages(PTE_W|PTE_U)`, in `vm.c`) maps the page, the handler increments the new `proc.pgfaults` field and the process resumes.
  - **Any other user-mode fault:** the handler prints the pid and the address and kills only that process. This covers addresses ≥ sz and protection faults such as the stack guard page.
  - **Kernel mode:** a lazy fault there (a system call touching an untouched user buffer) is handled the same way.
- **`vm.c`:**
  - `copyuvm()` skips missing or non-present PTEs instead of panicking.
  - `deallocuvm()` already skipped them; I added a comment.
  - `uva2ka()` checks for a missing page table.
  - `copyout()` allocates a lazy page of the current process, which is needed when `clone()` writes a thread's stack.
  - A spinlock (`lazylock`) stops two threads that share a page directory from mapping the same page twice.
- **Counter reset:** `allocproc()` sets `pgfaults` to 0. `exec()` also resets it, because the shell's child faults in heap pages while parsing the command, before it calls exec.
- **New system call:** `getfaults()` (number 31) returns `myproc()->pgfaults`.

**Output** (`q2_faulttest.log`).
- **Counts:** 0 faults at start; `sbrk(32*4096)` causes 0. Writing to pages 0, 7, 13, 21 and 31 causes exactly **5** faults, and writing again to pages 0 and 13 adds none.
- **Fork:** a fork with 27 untouched heap pages works. Before the fix it panicked in `copyuvm`. The child reads 0 from a page the parent never touched, and its own counter shows 1 fault.
- **Invalid accesses:** a write to `sz` (0x24000), to 0x7ff00000 and to the guard page (err 0x7, a protection fault) each kill only the child. The parent goes on to `ALL CHECKS PASSED`.

## Q3: FIFO and LRU (`pagerepl`)

**Program.** `pagerepl [NFRAMES PAGE ...]` is a user-space simulation; with no arguments it uses the assignment's string and 3 frames.
- **FIFO** evicts from a circular "oldest slot" pointer.
- **LRU** evicts the frame with the smallest last-use time.

For each reference the program prints the frames, and either "hit" or "FAULT" with the replaced page. It then prints a table for 3, 4 and 5 frames. The hit ratio is computed in integer hundredths, because xv6's `printf` has no floating point.

**Output** (`q3_pagerepl.log`), reference string `7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1`:

| Frames | FIFO faults | FIFO hit ratio | LRU faults | LRU hit ratio |
|---|---|---|---|---|
| 3 | **15** | 25.00% | **12** | 40.00% |
| 4 | 10 | 50.00% | 8 | 60.00% |
| 5 | 9 | 55.00% | 7 | 65.00% |

FIFO gives 15 faults and LRU gives 12 with 3 frames, as expected. LRU is never worse than FIFO here, and both improve as frames are added.

## Regression

On one boot, `usertests` (ALL TESTS PASSED), Lab 7 `threadtest` and `synctest`, `forktest` and the shell commands all pass (`regression.log`). The `mem` subtest's child is now killed with "out of memory for lazy page" rather than seeing `malloc` return 0, which is the expected effect of lazy `sbrk`.
