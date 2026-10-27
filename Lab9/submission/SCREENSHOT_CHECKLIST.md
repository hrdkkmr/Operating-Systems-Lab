# Screenshot checklist

## Already captured (genuine)

`xv6/runxv6.py --shots` asks the QEMU monitor for a `screendump` of the emulated VGA screen after each command, then converts the dump to PNG. The screen holds **25 lines**, so each image shows only the end of the output. The full output is in `output_logs/`.

| Question | Screenshot | Shows |
|---|---|---|
| Q1 | `screenshots/q1_pttest/01_pttest.png` | third dump (4 new PTEs 0x3000–0x6000) and `exactly 4 new pages appeared` |
| Q2 | `screenshots/q2_faulttest/01_faulttest.png` | fault count 5, fork checks, the three children killed, parent still running |
| Q3 | `screenshots/q3_pagerepl/01_pagerepl.png` | LRU trace end and the 3/4/5-frame table |
| Q3 | `screenshots/q3_pagerepl/02_…`, `03_…` | custom reference strings |
| Regression | `screenshots/regression/*.png` | shell commands, `usertests` ALL TESTS PASSED, Lab 7 tests, the Lab 9 programs |

## Optional: manual window screenshots

Some outputs are longer than 25 lines. To capture the start of `pttest` (dumps 1 and 2) or the FIFO trace of `pagerepl`, run `make qemu-nox` and screenshot the host terminal while scrolling back:

| Question | Command | Make sure the screenshot shows |
|---|---|---|
| Q1 | `pttest` | dumps [1] and [2] (3 pages, guard page `P W -`) and dump [3] (7 pages) |
| Q2 | `faulttest` | `faults so far 1..5`, then `5` again for the repeated writes |
| Q3 | `pagerepl` | `FIFO, 3 frames: 15 page faults` and `LRU, 3 frames: 12 page faults` |

Every file listed under "Already captured" comes from an actual QEMU run. No screenshot was edited.
