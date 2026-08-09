#!/usr/bin/env python3
# Tiny telnet-ish client for busybox telnetd on the tab (10.42.0.1:2323).
# usage: t4cmd.py "command" [wait_seconds] [host] [port]
import socket, sys, time

cmd = sys.argv[1] if len(sys.argv) > 1 else ''
wait = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
host = sys.argv[3] if len(sys.argv) > 3 else '10.42.0.1'
port = int(sys.argv[4]) if len(sys.argv) > 4 else 2323

s = socket.create_connection((host, port), timeout=5)
s.setblocking(False)
if cmd:
    s.sendall(cmd.encode() + b'\n')
end = time.time() + wait
buf = b''
while time.time() < end:
    try:
        d = s.recv(65536)
        if not d:
            break
        buf += d
    except BlockingIOError:
        time.sleep(0.1)
    except OSError:
        break
s.close()
# strip telnet IAC sequences and CRs
out = bytearray()
i = 0
while i < len(buf):
    if buf[i] == 0xFF and i + 2 < len(buf):
        i += 3
        continue
    if buf[i] != 0x0D:
        out.append(buf[i])
    i += 1
sys.stdout.buffer.write(bytes(out))
sys.stdout.flush()
