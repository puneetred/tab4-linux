# 11 — Networking (USB now, WiFi next)

## USB ethernet (works today)

The initramfs/rootfs brings up a **CDC-NCM** gadget: `usb0` on the tab is
`10.42.0.1`, and it runs `udhcpd` to give the host `10.42.0.100`. macOS drives
NCM natively, so plugging the cable just works.

* SSH: `ssh root@10.42.0.1` (password `t4`) — dropbear.
* Telnet (initramfs lifeline): `telnet 10.42.0.1 2323`.

## Getting packages onto the tab (no direct internet)

The tab has no upstream route by default. Options used:

1. **Fetch on the Mac, push over nc** (most reliable):
   `tools/t4pkgs.py <pkg...>` resolves an apk + deps from the Alpine index,
   downloads on the Mac, and `tools/t4push.py` pushes to a tab-side
   `nc -l -p 3333 > file`. Then `apk add --allow-untrusted *.apk`.
2. **HTTP proxy on the Mac** (`tools/t4proxy.py`) pointed the tab's
   `http_proxy` at it. (Only handles some cases.)
3. **SSH reverse tunnel** — *didn't work*: the dropbear build refuses
   remote forwarding (`AllowTcpForwarding`). Don't bother.

### nc direction matters (busybox vs GNU)

* **tab listens, Mac sends** (`t4push.py`) → reliable.
* **Mac serves a file, tab fetches** (`nc host < file` on Mac) → GNU netcat
  half-closes stdin and sends 0 bytes; use `cat file | nc -l port` — and even
  then it's flaky. Prefer direction #1.

## WiFi (sd8887 / mwifiex) — not yet up

Firmware is installed (`/lib/firmware/mrvl/sd8887_uapsta.bin`, etc.) and
`/etc/modules` loads `mlan`, `sd8887`. The OpenRC `sd8887.start` powers the
radio via `/sys/devices/platform/sd8x-rfkill/pwr_ctrl` and sets
`drv_mode=1`. Still TODO:

```sh
modprobe mlan; modprobe sd8887
echo 1 > /sys/devices/platform/sd8x-rfkill/pwr_ctrl
iw dev; ip link set mlan0 up
wpa_supplicant -B -i mlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
udhcpc -i mlan0
```

## Next distros (same boot/kernel/initramfs)

The boot stack is distro-agnostic — only `rootfs.img` changes:
* **postmarketOS** (Phosh/Plasma Mobile) — purpose-built for this device.
* **Debian armhf** bookworm, **sysvinit** (not systemd) + XFCE/LXQt — biggest
  app ecosystem. glibc 2.36 is fine on 3.10; just avoid systemd (needs ≥4.15).
* **Void armv7l** (runit) — rolling, systemd-free.
