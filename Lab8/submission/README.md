# Lab 8 — Memory Management in xv6 (submission)

The canonical sources are in `../xv6/`, the tree the Makefile builds. This folder holds only documentation and evidence, so there are no duplicate copies of the code that could drift out of sync.

## Where to look

| What | File |
|---|---|
| Consolidated report (design, measurements, Q8 table, problems) | [REPORT.md](REPORT.md) |
| Status per question / requirement, spec gaps for Q5–Q8 | [FINAL_STATUS.md](FINAL_STATUS.md) |
| How it was tested, exact commands, pass counts | [TESTING.md](TESTING.md) |
| Every new and modified file | [FILE_MANIFEST.md](FILE_MANIFEST.md) |
| Screenshots taken and how to take more | [SCREENSHOT_CHECKLIST.md](SCREENSHOT_CHECKLIST.md) |
| Full console output of each test run | [output_logs/](output_logs/) |
| QEMU screen captures per question | [screenshots/](screenshots/) |

## Questions

| Q | Topic | Program | Kernel interface |
|---|---|---|---|
| 1 | VA → PA translation | `vatest` | `int va2pa(int va)` |
| 2 | Physical memory statistics | `memtest` | `int getmeminfo(struct meminfo *mi)` (`meminfo.h`) |
| 3 | Page-table dump | `vmprinttest` | `int vmprint(void)` |
| 4 | Lazy heap allocation | `lazytest` (`lazytest oom`) | lazy `sbrk`, `T_PGFLT` handler |
| 5* | Copy-on-write fork | `cowtest` | `fork()` |
| 6* | Memory protection | `protecttest` | `int mprotect(void*, int)`, `int munprotect(void*, int)` |
| 7* | Shared memory | `shmtest` | `void *shm_open(int key)`, `int shm_close(int key)` |
| 8* | FIFO/LRU/Optimal | `pagerepl` | user-space simulation |

\* Inferred extensions: the PDF names these programs but gives no task text (see `FINAL_STATUS.md`).

## Build and run

```sh
cd ../xv6
make clean && make fs.img xv6.img
make qemu-nox           # then type e.g.  vatest
sh runlab8.sh           # automated: all tests, logs + screenshots into ../submission
```
