#!/usr/bin/env python3
# Tiny HTTP forward proxy so the tab (10.42.0.1) can reach Alpine's CDN
# through the Mac's internet connection. Plain-HTTP only, absolute-URI GETs.
import socket, threading, re, sys

def handle(conn):
    try:
        conn.settimeout(60)
        data = b''
        while b'\r\n\r\n' not in data:
            chunk = conn.recv(8192)
            if not chunk:
                conn.close(); return
            data += chunk
        head, _, rest = data.partition(b'\r\n\r\n')
        lines = head.split(b'\r\n')
        m = re.match(rb'(GET|POST|HEAD|PUT)\s+(\S+)\s+(HTTP/\d\.\d)', lines[0])
        if not m:
            conn.close(); return
        method, uri, ver = m.groups()
        print('REQ', method, uri[:120], flush=True)
        hostport = b''
        path = uri
        if uri.startswith(b'http://'):
            u = uri[7:]
            hostport, _, p = u.partition(b'/')
            path = b'/' + p
        else:
            for l in lines[1:]:
                if l.lower().startswith(b'host:'):
                    hostport = l[5:].strip()
        host, _, ps = hostport.partition(b':')
        port = int(ps) if ps else 80
        ip = socket.gethostbyname(host.decode())
        out = socket.create_connection((ip, port), timeout=30)
        newhead = [method + b' ' + path + b' ' + ver]
        for l in lines[1:]:
            ll = l.lower()
            if ll.startswith(b'connection:') or ll.startswith(b'proxy-') or ll.startswith(b'keep-alive'):
                continue
            newhead.append(l)
        newhead.append(b'Connection: close')
        out.sendall(b'\r\n'.join(newhead) + b'\r\n\r\n' + rest)
        while True:
            chunk = out.recv(65536)
            if not chunk:
                break
            conn.sendall(chunk)
        out.close()
    except Exception as e:
        print('ERR', repr(e), flush=True)
    finally:
        try: conn.close()
        except Exception: pass

s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('0.0.0.0', 8888))
s.listen(64)
print('t4proxy listening on 0.0.0.0:8888', flush=True)
while True:
    c, _ = s.accept()
    threading.Thread(target=handle, args=(c,), daemon=True).start()
