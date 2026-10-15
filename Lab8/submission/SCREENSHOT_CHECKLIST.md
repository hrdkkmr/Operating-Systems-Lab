# Screenshot checklist

## Already captured (genuine)

`xv6/runxv6.py --shots` asks the QEMU monitor for a `screendump` of the emulated VGA screen after each command, then converts it to PNG. xv6 prints to both the serial port and the CGA text screen, so these images are the virtual machine's real display. The screen holds **25 lines**, so each image shows the end of the output: the last checks and the `ALL CHECKS PASSED` line. The complete output is in `output_logs/`.

| Question | Screenshot | Shows |
|---|---|---|
| Q1 | `screenshots/q1_vatest/01_vatest.png` | fork part: parent/child PA before and after the write; final result |
| Q2 | `screenshots/q2_memtest/01_memtest.png`, `02_memtest.png` | fork/reap, shrink and leak-test numbers; second run with identical baseline |
| Q3 | `screenshots/q3_vmprinttest/01_vmprinttest.png` | last dump (after `sbrk(-4096)`) with the guard page and summary lines |
| Q4 | `screenshots/q4_lazytest/01_lazytest.png`, `02_lazytest_oom.png` | invalid accesses killed, OOM child killed, shrink results |
| Q5 | `screenshots/q5_cowtest/01_cowtest.png` | COW checks and leak test |
| Q6 | `screenshots/q6_protecttest/01_protecttest.png` | protection faults killing only the child |
| Q7 | `screenshots/q7_shmtest/01_shmtest.png` | parent/child/exec'ed peer sharing, cleanup |
| Q8 | `screenshots/q8_pagerepl/01_pagerepl.png`, `02_pagerepl_3_1_2_3_4_1_2_5.png` | FIFO/LRU/Optimal table; custom string |
| Regression | `screenshots/regression/*.png` | shell commands, `usertests` ALL TESTS PASSED, Lab 7 tests, all Lab 8 programs |

## Optional: manual window screenshots

For a screenshot of a whole terminal window, or of the beginning of a long output, run the tests interactively and capture them yourself:

```sh
cd Lab8/xv6
make qemu-nox        # (or `make qemu` for a QEMU window)
```

| Question | Command | Make sure the screenshot shows |
|---|---|---|
| Q1 | `vatest` | the global/local/malloc VA→PA lines with `[va2pa]` PDX/PTX/offset/P W U; `0x7FFFFFFF → -1` |
| Q2 | `memtest` | `[1] baseline`, `[2] after sbrk`, `[3]` fork values, `[PASS] free_pages back to the pre-fork value` |
| Q3 | `vmprinttest` | dump 1 with `<- stack guard page (PTE_U clear)` and the `user:`/`kernel:` summary lines; dumps 3 and 4 (PTEs 4–11 appear, PTE 11 disappears) |
| Q4 | `lazytest` | `[2]` free_pages unchanged after `sbrk(32 MB)`; `[3]` only 4 data + 3 page-table pages used; `[7]` kill messages |
| Q4 | `lazytest oom` | `out of physical memory -- kill proc` and `all its frames were reclaimed` |
| Q5 | `cowtest` | `[1]` and `[2]` (same frame after fork; one copy per written page) |
| Q6 | `protecttest` | `[2]`–`[4]` |
| Q7 | `shmtest` | `[2]` and `[3]` messages from the child and the peer |
| Q8 | `pagerepl` | the summary table |

With `make qemu-nox` the output goes to your host terminal, so its scroll-back lets you capture long outputs in several screenshots. To leave QEMU: `Ctrl-a x`.

All files listed under "Already captured" were produced by actual QEMU runs. No screenshot was edited or made up.
