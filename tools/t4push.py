#!/usr/bin/env python3
# Push a file to a busybox `nc -l -p PORT` listener on the tab, reliably.
# usage: t4push.py <file> [port] [host]
import socket, sys, time

path = sys.argv[1]
port = int(sys.argv[2]) if len(sys.argv) > 2 else 3333
host = sys.argv[3] if len(sys.argv) > 3 else '10.42.0.1'

data = open(path, 'rb').read()
s = socket.create_connection((host, port), timeout=15)
s.settimeout(300)
s.sendall(data)
s.shutdown(socket.SHUT_WR)          # orderly FIN so server sees EOF after data
# drain until server closes (ensures full delivery)
s.settimeout(30)
got = b''
try:
    while True:
        d = s.recv(4096)
        if not d:
            break
        got += d
except socket.timeout:
    pass
s.close()
print('sent', len(data), 'bytes;', 'server replied:', got[:200])
