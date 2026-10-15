#!/bin/sh
# Lab 8: build xv6 and run every Lab 8 test program in QEMU, one fresh
# boot per question, saving the full console output and VGA screenshots.
# Run from the xv6 directory (Linux/WSL with gcc, make, qemu-system-i386,
# python3):   sh runlab8.sh [OUTDIR]        (default OUTDIR ../submission)
set -e
OUT=${1:-../submission}
LOGS=$OUT/output_logs
SHOTS=$OUT/screenshots
mkdir -p "$LOGS" "$SHOTS"

make fs.img xv6.img

run() {   # run NAME CMD...
  name=$1; shift
  echo "=== $name: $*"
  python3 runxv6.py "$LOGS/$name.log" --timeout 900 --shots "$SHOTS/$name" "$@" \
    > /dev/null || echo "!!! $name: runner reported a timeout"
  grep -E "ALL CHECKS PASSED|SOME CHECKS FAILED|ALL TESTS PASSED|panic|TIMEOUT" \
    "$LOGS/$name.log" || true
}

run q1_vatest      vatest
run q2_memtest     memtest memtest
run q3_vmprinttest vmprinttest
run q4_lazytest    lazytest "lazytest oom"
run q5_cowtest     cowtest
run q6_protecttest protecttest
run q7_shmtest     shmtest
run q8_pagerepl    pagerepl "pagerepl 3 1 2 3 4 1 2 5"
run regression     ls "echo hello xv6" "cat README | wc" "mkdir d1" "ls d1" \
                   forktest usertests threadtest synctest \
                   vatest memtest vmprinttest lazytest cowtest protecttest \
                   shmtest pagerepl memtest
