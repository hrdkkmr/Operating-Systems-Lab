# Lab 9: Page Tables, Page Faults and Page Replacement in xv6 (submission)

The source code lives only in `../xv6/`, the tree the Makefile builds. This folder holds just the documentation and test evidence, so there is no second copy of the code to fall out of sync.

## Where to look

| What | File |
|---|---|
| Short report: what changed and what the output shows | [REPORT.md](REPORT.md) |
| Status of each question and requirement | [FINAL_STATUS.md](FINAL_STATUS.md) |
| How it was tested: commands and results | [TESTING.md](TESTING.md) |
| Every new and modified file | [FILE_MANIFEST.md](FILE_MANIFEST.md) |
| Screenshots taken, and how to take more | [SCREENSHOT_CHECKLIST.md](SCREENSHOT_CHECKLIST.md) |
| Full console output of each test run | [output_logs/](output_logs/) |
| QEMU screen captures for each question | [screenshots/](screenshots/) |

## Questions

| Q | Topic | Program | Kernel interface |
|---|---|---|---|
| 1 | Printing the page table | `pttest` | `int vmprint(void)` (syscall 30) |
| 2 | Lazy allocation / page faults | `faulttest` | lazy `sbrk`, `T_PGFLT` handler, `int getfaults(void)` (syscall 31) |
| 3 | FIFO and LRU page replacement | `pagerepl [NFRAMES PAGE ...]` | user-space simulation |

## Build and run

```sh
cd ../xv6
make clean && make fs.img xv6.img
make qemu-nox           # then type: pttest, faulttest, pagerepl
sh runlab9.sh           # automated: all tests, logs + screenshots into ../submission
```
