#!/bin/sh
# Regenerate submission/src/ from the canonical xv6 sources and build
# Lab7/submission.zip (sources, docs, logs, screenshots; no build products).
# Run from anywhere: sh Lab7/submission/make_submission.sh
set -e
here=$(cd "$(dirname "$0")" && pwd)
lab=$(dirname "$here")
xv6="$lab/xv6"

rm -rf "$here/src"
mkdir -p "$here/src/user" "$here/src/kernel"
for f in threadtest.c synctest.c deadlock.c priotest.c; do
  cp "$xv6/$f" "$here/src/user/"
done
for f in proc.c proc.h semaphore.c exec.c trap.c sysproc.c syscall.c x86.h \
         syscall.h usys.S defs.h user.h param.h Makefile; do
  cp "$xv6/$f" "$here/src/kernel/"
done
# Diff of the whole xv6 tree against the starting commit (54490cf).
# core.autocrlf=true: the repository stores CRLF text, the working files are
# LF; without it every line would show as changed.
( cd "$lab" && git -c core.autocrlf=true diff --no-color 54490cf -- xv6 ) \
  > "$here/src/kernel.patch"

cd "$lab"
rm -f submission.zip
if command -v zip >/dev/null 2>&1; then
  zip -qr submission.zip submission -x 'submission/*.zip'
else
  python3 - <<'EOF'
import os, zipfile
with zipfile.ZipFile("submission.zip", "w", zipfile.ZIP_DEFLATED) as z:
    for root, _, files in os.walk("submission"):
        for f in files:
            if not f.endswith(".zip"):
                z.write(os.path.join(root, f))
EOF
fi
echo "wrote $lab/submission.zip"
