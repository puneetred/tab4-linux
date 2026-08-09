# 99 — Debugging methodology

A recurring theme in this port: the obvious symptom pointed at the wrong
subsystem. Each fix required obtaining ground truth from the device rather than
reasoning from the symptom alone. The techniques below are listed roughly in the
order they proved useful.

## Principles

1. **Get a debug channel that can't be taken away.** The initramfs telnetd runs
   from RAM and survives `switch_root`. Every later fix was done over `usb0`.
2. **Never trust a "success" without readback.** A `dd` to a nonexistent
   `/dev/block/...` path "succeeded" and cost a flash cycle. Now everything is
   verified by mount/`cmp`.
3. **Same symptom ≠ same cause.** Three different bugs all looked like
   `Illegal instruction ...674`. Assume nothing.

## Techniques that mattered

### Map a crash address to a file *without* gdb
Xorg printed `Illegal instruction at 0xb6XX674`. To find the owning library:
* grab a core (`ulimit -c unlimited`, `kernel.core_pattern=/tmp/core`), then
* parse the ELF core's **NT_FILE note** in python → vaddr → `path + file offset`.
* That's how we proved the crash was in **ld-musl `a_crash()`**, not libpixman.
  (Full details: docs/07, Bug 1.)

### Disassemble the exact faulting instruction
`arm-linux-gnueabihf-objdump -d --start-address=... --stop-address=... <lib>`.
The `e7f000f0 (udf #0)` = musl's deliberate `a_crash()` told us it was a heap
trap, not a CPU feature problem.

### Read hardware registers directly
The audio codec has no DAPM, so amixer "values" lie (broken readback). Ground
truth is the regmap debugfs:
```sh
cat /sys/kernel/debug/regmap/2-0038/registers   # codec page
cat /sys/kernel/debug/regmap/2-0030/registers   # PMIC page (Class-D amp)
```
Seeing `0x50/0x80 = 0x00` proved the whole output path was powered off.

### Reuse the postmarketOS device port
This device has a pmOS port. `pmaports/device/archived/device-samsung-degaswifi/`
gave us: the kernel patch set, the WiFi firmware list, the modprobe confs, and —
critically — the **UCM2 audio register sequence** that no amount of
register-guessing would have produced.

### Capture live input events
`xinput test-xi2 --root` (needs `XAUTHORITY=/tmp/serverauth.*` when run as root)
showed: touch produced `RawTouchBegin` but **0 ButtonPress** (evdev/XI2 quirk),
and power keycode 124 *did* reach X (so the "dead" power button was really
xfce4-power-manager swallowing it).

### Screenshots over nc
`fbgrab /tmp/s.png` on the tab, then `nc` the PNG to the Mac to actually *see*
the desktop (black screen vs rendered desktop vs greeter).

## Host-side tooling (tools/)

* `t4push.py` — reliable Mac→tab file push (orderly FIN + drain). The busybox
  nc listener must be `setsid`-detached or it dies with the ssh session.
* `t4ssh.exp` — password-ssh runner (dropbear allows password auth).
* `t4cmd.py` — telnet client that strips IAC bytes (initramfs shell).
* `t4proxy.py` / `t4tunnel.exp` — package-fetch plumbing (reverse tunnel is
  blocked by dropbear; don't rely on it).

## If you only remember three things

1. Keep the RAM-resident telnetd alive no matter what.
2. Verify every flash by reading it back.
3. When Xorg SIGILLs at `...674`, suspect musl `a_crash` from a bad probe path —
   not the CPU.
