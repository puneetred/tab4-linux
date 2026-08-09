#!/usr/bin/env python3
# Minimal non-blocking serial talker for the tab's ACM gadget shell.
# usage: ser.py [port] [command] [wait_seconds]
import os, sys, termios, select, time

port = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbmodem14103'
cmd = sys.argv[2] if len(sys.argv) > 2 else ''
wait = float(sys.argv[3]) if len(sys.argv) > 3 else 2.0

fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
a = termios.tcgetattr(fd)
a[0] = 0                                   # iflag
a[1] = 0                                   # oflag
a[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # cflag
a[3] = 0                                   # lflag
a[4] = termios.B115200                     # ispeed
a[5] = termios.B115200                     # ospeed
termios.tcsetattr(fd, termios.TCSANOW, a)
os.set_blocking(fd, False)
# drain stale input
try:
    while os.read(fd, 65536):
        pass
except BlockingIOError:
    pass
if cmd:
    os.write(fd, cmd.encode() + b'\n')
end = time.time() + wait
out = b''
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.2)
    if r:
        try:
            d = os.read(fd, 65536)
        except BlockingIOError:
            d = b''
        if d:
            out += d
sys.stdout.buffer.write(out)
sys.stdout.flush()
os.close(fd)
