#!/usr/bin/env python3
# Lab 8 test driver (reused unchanged in Lab 9): boots xv6 in QEMU (no window, serial console on
# stdin/stdout), waits for the shell prompt, types each command, waits for
# the next prompt and records everything xv6 prints.  QEMU is always
# killed at the end, so a hung test cannot leave a QEMU process behind.
#
# usage: python3 runxv6.py LOGFILE [--timeout SECS] [--shots DIR] CMD ...
#
# --shots DIR: after each command, ask the QEMU monitor for a
# "screendump" of the emulated VGA text screen (xv6 prints to both the
# serial port and the CGA screen) and save it as DIR/<n>_<cmd>.png.
# These are real captures of the virtual machine's display; the screen
# holds the last 25 lines of output.
import os, re, select, socket, struct, subprocess, sys, tempfile, time, zlib

def ppm_to_png(src, dst):
    data = open(src, "rb").read()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    w, h = int(m.group(1)), int(m.group(2))
    pix = data[m.end():m.end() + w * h * 3]
    raw = b"".join(b"\x00" + pix[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = t + d
        return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c))
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    open(dst, "wb").write(png)

def main():
    args = sys.argv[1:]
    log = args.pop(0)
    timeout = 120
    shots = None
    while args and args[0].startswith("--"):
        opt = args.pop(0)
        if opt == "--timeout":
            timeout = int(args.pop(0))
        elif opt == "--shots":
            shots = args.pop(0)
    cmds = args
    mon = os.path.join(tempfile.mkdtemp(), "mon.sock")
    qemu = ["qemu-system-i386", "-display", "none", "-serial", "stdio",
            "-monitor", "unix:%s,server,nowait" % mon,
            "-drive", "file=fs.img,index=1,media=disk,format=raw",
            "-drive", "file=xv6.img,index=0,media=disk,format=raw",
            "-smp", "2,sockets=2,cores=1,threads=1", "-m", "512"]
    p = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT)
    out = bytearray()
    status = 0

    def wait_prompt(start, limit):
        # The shell prints "$ " when it is ready for the next command.
        deadline = time.time() + limit
        while time.time() < deadline:
            r, _, _ = select.select([p.stdout], [], [], 0.2)
            if r:
                chunk = os.read(p.stdout.fileno(), 4096)
                if not chunk:
                    return False
                out.extend(chunk)
                if out.rfind(b"$ ") >= start and out.endswith(b"$ "):
                    return True
        return False

    def screendump(n, cmd):
        name = "%02d_%s" % (n, re.sub(r"[^A-Za-z0-9]+", "_", cmd).strip("_"))
        ppm = os.path.join(tempfile.gettempdir(), name + ".ppm")
        s = socket.socket(socket.AF_UNIX)
        s.connect(mon)
        s.sendall(("screendump %s\n" % ppm).encode())
        time.sleep(1.0)
        s.close()
        os.makedirs(shots, exist_ok=True)
        ppm_to_png(ppm, os.path.join(shots, name + ".png"))
        os.remove(ppm)

    try:
        if not wait_prompt(0, 60):
            out.extend(b"\n[runxv6] TIMEOUT waiting for boot prompt\n")
            status = 1
        else:
            for i, c in enumerate(cmds, 1):
                start = len(out)
                p.stdin.write((c + "\n").encode())
                p.stdin.flush()
                if not wait_prompt(start + len(c) + 1, timeout):
                    out.extend(("\n[runxv6] TIMEOUT after %ds running: %s\n"
                                % (timeout, c)).encode())
                    # Ctrl-P: ask xv6 for a process dump (procdump) to
                    # show where it is stuck.
                    p.stdin.write(b"\x10")
                    p.stdin.flush()
                    wait_prompt(len(out), 5)
                    status = 1
                    break
                if shots:
                    time.sleep(0.5)
                    screendump(i, c)
    finally:
        p.kill()
        p.wait()
    text = out.decode("utf-8", "replace").replace("\r", "")
    with open(log, "w") as f:
        f.write(text)
    sys.stdout.write(text)
    sys.exit(status)

main()
