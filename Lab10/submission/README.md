# Lab 10 — Memory Management, Virtual Memory, Paging and Page Replacement

**Course:** Operating Systems  
**Assignment:** Lab 10  
**Base:** xv6 x86, built on Lab 9 (which includes lazy `sbrk`, `vmprint`, threads, semaphores, priority scheduling)

---

## Objective

1. **Q1:** Track physical page allocation and deallocation; expose statistics via `memstats()`.
2. **Q2:** Translate user virtual addresses to physical addresses via `vtop()`.
3. **Q3:** Inspect page-table entries (`pageinfo()`) and enforce read-only protection (`setreadonly()`).
4. **Q4:** Simulate and compare FIFO, LRU, and Optimal page replacement algorithms.

---

## Submission Structure

```
Lab10/
├── OS_Assignment_10.pdf        assignment specification
├── README.md                   top-level overview
└── xv6/                        canonical xv6 source tree (all changes here)
    ├── kalloc.c                ← modified (Q1)
    ├── vm.c                    ← modified (Q2, Q3)
    ├── sysproc.c               ← modified (Q1–Q3)
    ├── syscall.c               ← modified (Q1–Q3)
    ├── syscall.h               ← modified (Q1–Q3)
    ├── defs.h                  ← modified (Q1–Q3)
    ├── user.h                  ← modified (Q1–Q3)
    ├── usys.S                  ← modified (Q1–Q3)
    ├── Makefile                ← modified (new UPROGS)
    ├── memtest.c               new — Q1 test program
    ├── vttest.c                new — Q2 test program
    ├── pagetest.c              new — Q3 test program
    └── pagereplace.c           new — Q4 simulation

submission/
    ├── REPORT.md               ← 1-2 page report (implementation + observations)
    ├── FILE_MANIFEST.md        ← complete file list with change descriptions
    ├── TESTING.md              ← build/run steps and expected results
    ├── SCREENSHOT_CHECKLIST.md ← what each screenshot must show
    ├── FINAL_STATUS.md         ← per-question status and regression notes
    ├── src/
    │   ├── kernel/             copies of all 9 modified kernel/build files
    │   └── user/               copies of all 4 new user programs
    ├── output_logs/            full QEMU serial console output per test
    └── screenshots/            VGA screen captures per question
```

---

## New System Calls

| Number | Signature | Purpose |
|---|---|---|
| 32 | `int memstats(void)` | Print physical memory statistics; return current pages in use |
| 33 | `int vtop(int va)` | Return physical address for virtual address `va`; -1 if unmapped |
| 34 | `int pageinfo(int va)` | Print physical frame number and P/W/U flags for `va` |
| 35 | `int setreadonly(int va)` | Clear PTE_W for the page containing `va`; flush TLB |

---

## How to Build and Run

```bash
# From Lab10/xv6 in Linux/WSL with gcc + make + qemu-system-i386:
make clean
make fs.img xv6.img
make qemu-nox

# Inside xv6 shell:
$ memtest
$ vttest
$ pagetest
$ pagereplace
```

Exit QEMU: `Ctrl-a x`

---

## Quick Results Summary

| Test | Key result |
|---|---|
| `memtest` | Page count rises by exactly 16 after touching 16 sbrk pages → `[PASS]` |
| `vttest` | Stack, heap, mid-page return valid PAs; unmapped returns -1 → all `[PASS]` |
| `pagetest` | W flag drops 1→0; child write triggers trap 14 → `[PASS]` |
| `pagereplace` | FIFO=15, LRU=12, OPT=9 faults with 3 frames (reference string of 20 refs) |
