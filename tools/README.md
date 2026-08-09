# tools/ — macOS-side helpers

The tab is reachable over the USB-NCM link at `10.42.0.1`. These wrap the
quirky bits (busybox nc, dropbear password auth, telnet IAC).

| Tool | Purpose |
|---|---|
| `t4push.py <file> [port] [host]` | Reliable Mac→tab file push to a tab-side `nc -l -p PORT > file`. Orderly FIN + drain so big files land intact. |
| `t4cmd.py "<cmd>" [wait]` | Minimal telnet client for the initramfs busybox shell (`:2323`). Strips IAC bytes. Works before ssh is up. |
| `t4ssh.exp "<cmd>"` | password-ssh runner (dropbear allows password auth; root/t4). |
| `t4tunnel.exp` | ssh reverse-tunnel attempt — **dropbear refuses remote forwarding**; kept for reference, don't rely on it. |
| `t4proxy.py` | tiny HTTP proxy so the tab can `apk` through the Mac (partial). |
| `t4pkgs.py <pkg...>` | resolve an apk + deps from the Alpine index, download on the Mac, push to the tab. |
| `ser.py` | serial-console helper (USB gadget ACM) used during early bring-up. |

## Patterns that work

```sh
# push a file to the tab
expect tools/t4ssh.exp "setsid sh -c 'nc -l -p 3333 > /root/f' </dev/null >/dev/null 2>&1 & sleep 2; echo up"
python3 tools/t4push.py ./f 3333 10.42.0.1

# run a command on the tab
expect tools/t4ssh.exp 'uname -a'
```

**nc direction:** tab-listens/Mac-sends (t4push) is reliable. Mac-serves/tab-fetches
(`nc host < file`) is flaky under GNU netcat (stdin half-close → 0 bytes).
