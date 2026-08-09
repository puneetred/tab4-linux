# 04 — Initramfs (the debug lifeline)

Built by `build/scripts/40-initramfs.sh` from `build/initramfs-skel/`. Static
busybox + an `init` script. Its job is to get a debug channel up *before* the
real rootfs, so we never lose the device to a bad rootfs.

## init logic (build/initramfs-skel/init)

1. Install busybox applets, mount devtmpfs/proc/sys/devpts.
2. **USB network first** (`setup_usb_debug`):
   * `insmod /lib/g_ncm.ko` → wait for `usb0` → `ifconfig usb0 10.42.0.1/24` →
     `udhcpd` (hands the host 10.42.0.100) → `telnetd -l /bin/sh -p 2323`.
   * Because telnetd runs from RAM, it **survives switch_root** — you can still
     get a shell even if the desktop later wedges.
   * Fallback path configures the Samsung android gadget (rndis,acm) if NCM
     is unavailable.
3. Wait for `/dev/mmcblk0p15`, mount it, and **only if** it has the marker
   `/etc/t4-rootfs` and an executable `/sbin/init`, `switch_root` into it.
4. Otherwise drop to a framebuffer-console shell loop (`cttyhack sh`) so you can
   debug on the panel itself.

## Why the marker file

Early iterations risked `switch_root`ing into the *Android* system (which has
no `/sbin/init` for our purposes). The `t4-rootfs` marker makes the decision
explicit — only a rootfs we built gets switched into.

## Debug channels once booted

```sh
telnet 10.42.0.1 2323        # busybox shell (from initramfs, always there)
ssh root@10.42.0.1           # dropbear in the real rootfs (password t4)
```

`tools/t4cmd.py` is a minimal telnet client that strips IAC sequences (used to
run on-device commands from scripts before ssh was up).
