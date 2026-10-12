#!/usr/bin/env python3
"""Boot xv6 headless in QEMU, type shell commands, and record the console.

Usage (needs qemu-system-i386 and a built xv6 directory):
  python3 run_xv6.py --xv6 ../../xv6 [--cpus N] [--timeout SECS]
                     [--log FILE] [--shots DIR] [--accel ACCEL] cmd...

Each command is typed at the xv6 "$ " prompt; the script waits for the next
prompt (or the per-command timeout) before sending the next command.  QEMU
is always killed at the end, so a hung test cannot hang the session.

--log FILE   raw serial console output, byte for byte (nothing is added).
--shots DIR  after each command, save a PNG of the emulated VGA screen
             (QEMU monitor "screendump"); xv6 prints to VGA and serial alike,
             so the picture shows the last 25 console lines.

If no prompt is seen and the console has been silent for --quiet seconds
(default 120), the harness types "echo __done_N__" and treats that output as
proof the shell is back (on SMP the "$ " prompt can be interleaved with
another process's output).  The typed marker is visible in the log.

Harness notes (timing, timeouts, screenshot paths) go to stdout only, never
into the log file.  Exit status is 1 if xv6 did not boot or a command timed
out.

Example:
  python3 run_xv6.py --xv6 ../../xv6 --cpus 2 --log q1.log threadtest
"""
import argparse, os, re, select, socket, subprocess, sys, tempfile, time

ap = argparse.ArgumentParser()
ap.add_argument("--cpus", type=int, default=2)
ap.add_argument("--timeout", type=float, default=300.0)
ap.add_argument("--log", default=None)
ap.add_argument("--shots", default=None)
ap.add_argument("--xv6", default=".")
ap.add_argument("--accel", default=None, help="e.g. tcg,thread=multi")
ap.add_argument("--quiet", type=float, default=120.0,
                help="silence (s) before the marker fallback")
ap.add_argument("cmds", nargs="*")
a = ap.parse_args()

# Same topology as the Makefile: one core per socket, otherwise SeaBIOS's
# MP table lists only one CPU and xv6 boots on a single CPU.
qemu = ["qemu-system-i386", "-no-reboot",
        "-drive", "file=fs.img,index=1,media=disk,format=raw",
        "-drive", "file=xv6.img,index=0,media=disk,format=raw",
        "-smp", "%d,sockets=%d,cores=1,threads=1" % (a.cpus, a.cpus),
        "-m", "512"]
mon_path = None
if a.shots:
    a.shots = os.path.abspath(a.shots)
    os.makedirs(a.shots, exist_ok=True)
    mon_path = os.path.join(tempfile.mkdtemp(), "mon.sock")
    qemu += ["-display", "none", "-serial", "stdio",
             "-monitor", "unix:%s,server,nowait" % mon_path]
else:
    qemu += ["-nographic"]
if a.accel:
    qemu += ["-accel", a.accel]

p = subprocess.Popen(qemu, cwd=a.xv6, stdin=subprocess.PIPE,
                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
buf = bytearray()
logf = open(a.log, "wb") if a.log else None


QUIET = a.quiet              # seconds of silence before the marker fallback
last_out = [time.time()]     # time of the most recent console output


def pump(deadline, want, start):
    """Read output until `want` appears after offset `start` or deadline."""
    while time.time() < deadline:
        r, _, _ = select.select([p.stdout], [], [], 0.2)
        if r:
            chunk = os.read(p.stdout.fileno(), 4096)
            if not chunk:
                return False
            last_out[0] = time.time()
            buf.extend(chunk)
            sys.stdout.buffer.write(chunk)
            sys.stdout.flush()
            if logf:
                logf.write(chunk)
                logf.flush()
        if buf.find(want, start) >= 0:
            return True
    return False


def screendump(name):
    path = os.path.join(a.shots, name)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(mon_path)
    s.settimeout(5)
    try:
        s.recv(4096)                       # monitor banner
    except socket.timeout:
        pass
    s.sendall(("screendump %s -f png\n" % path).encode())
    time.sleep(1.0)
    s.close()
    print("\n[harness] screenshot saved: %s" % path)


status = 0
try:
    if not pump(time.time() + 120, b"$ ", 0):
        print("\n[harness] xv6 did not reach the shell prompt")
        status = 1
    else:
        for i, cmd in enumerate(a.cmds):
            start = len(buf)
            p.stdin.write(cmd.encode() + b"\n")
            p.stdin.flush()
            t0 = time.time()
            if not pump(time.time() + 30, b"\n", start):
                status = 1
                break
            # The prompt we wait for is the one after the echoed command.
            echo_end = buf.find(b"\n", start) + 1
            deadline = time.time() + a.timeout
            ok = False
            marker = None
            while time.time() < deadline:
                if marker is None:
                    if pump(min(deadline, time.time() + 1), b"$ ", echo_end):
                        ok = True
                        break
                    # On SMP the shell's "$ " can be interleaved character
                    # by character with another process's output (e.g.
                    # init's "zombie!" became "zomb$i e!"), so the literal
                    # prompt never appears.  After QUIET seconds without
                    # any output, type a marker command: the shell runs it
                    # only once the current command has finished, so its
                    # output proves completion.  The marker is in the log.
                    if time.time() - last_out[0] >= QUIET:
                        marker = b"__done_%d__" % (i + 1)
                        print("\n[harness] no prompt for %ds; typing marker"
                              % QUIET)
                        mstart = len(buf)
                        p.stdin.write(b"echo " + marker + b"\n")
                        p.stdin.flush()
                else:
                    pump(min(deadline, time.time() + 1), b"\x00", len(buf))
                    # Output of the echo command starts a line; the typed
                    # echo of the input starts with "echo ".
                    if re.search(rb"[\r\n]" + marker, bytes(buf[mstart:])):
                        ok = True
                        break
            if not ok:
                print("\n[harness] TIMEOUT after %.0fs waiting for '%s'"
                      % (a.timeout, cmd))
                status = 1
                break
            print("\n[harness] '%s' finished in %.1fs wall time"
                  % (cmd, time.time() - t0))
            if a.shots:
                time.sleep(0.5)
                safe = re.sub(r"[^A-Za-z0-9]+", "_", cmd).strip("_")
                screendump("%02d_%s.png" % (i + 1, safe))
finally:
    p.kill()
    p.wait()
    if logf:
        logf.close()
sys.exit(status)
