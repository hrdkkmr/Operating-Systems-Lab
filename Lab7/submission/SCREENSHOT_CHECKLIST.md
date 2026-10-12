# Screenshot checklist

`screenshots/` holds PNG captures of the emulated VGA screen, taken with QEMU's
`screendump` monitor command by `tools/run_xv6.py --shots` right after each
command finished. They show the real xv6 console, but only its **last 25
lines**. The complete output of the same runs is in `output_logs/`.

If the evaluator wants screenshots of your own terminal window, take the
following, one per item. Use `make qemu-nox` from `Lab7/xv6`, maximize the
window, and set a scroll-back large enough for the whole output.

| # | Requirement shown | Steps | Must be visible |
|---|---|---|---|
| 1 | Boot on 2 CPUs | `make clean && make && make fs.img && make qemu-nox` | `cpu1: starting 1`, `cpu0: starting 0`, `$` prompt |
| 2 | Q1 threads, race | `threadtest 10` | `created thread … (tid …)`, `joined tid …`, run lines with counters below 300000, `ALL CHECKS PASSED` |
| 3 | Q1 kernel checks | same run, scrolled to `Checks:` | the 11 `[PASS]` lines |
| 4 | Q2 counter | `synctest`, top part | unsynchronized counter < 300000, synchronized `300000 of 300000`, `[PASS]` |
| 5 | Q2 semaphore API | same run | the 9 API `[PASS]` lines |
| 6 | Q2 producer/consumer | same run, Part B | `buffer holds 5/5`, consumer lines, summary, 4 `[PASS]` lines |
| 7 | Q3 scenario 1 | `deadlock matrix`, top | matrices, `pass` lines, `DEADLOCKED` lines |
| 8 | Q3 scenarios 2 and 3 | same run, scrolled | safe sequence `<P1 P3 P4 P0 P2>`, `request GRANTED`, `UNSAFE: request DENIED, rolled back` |
| 9 | Q3 real circular wait | `deadlock procs` | pids of A and B, `still blocked`, `CYCLE`, `killing victim`, `saw EOF`, `ALL CHECKS PASSED`, and a `$` prompt afterwards |
| 10 | Q4 aging | `priotest`, Experiment 1 | the `aging:` trace and the table with `20 -> …` |
| 11 | Q4 round robin | same run, Experiment 2 | finish times close together, `ALL CHECKS PASSED` |
| 12 | Q4 on one CPU (optional) | `make qemu-nox CPUS=1`, then `priotest` | first-run times about 150/307/464 ticks for priorities 10/15/20 |
| 13 | Regression | `usertests` on a fresh `fs.img` | `ALL TESTS PASSED` |

Tips:

* Under Windows, `Win+Shift+S` captures a region of the terminal.
* For long outputs, either take several screenshots while scrolling, or run
  the command with fewer iterations (for example `threadtest 3`).
* Do not edit the output. If a check fails, capture it as it is and note it.
