# tab4-linux

A Linux port for the Samsung Galaxy Tab 4 7.0 (SM-T231, Marvell PXA1088),
consisting of a custom 3.10 kernel, a minimal initramfs, an Alpine Linux armhf
root filesystem, and an Xorg/XFCE4 graphical desktop.

The port replaces the stock Android system and does not depend on any Android
components at runtime. It was developed on a device with a non-functional
Android installation and is intended for hardware you are prepared to erase.

---

## Device support

| Component | Details |
|---|---|
| Model | Samsung Galaxy Tab 4 7.0, SM-T231 (`degas3g`) |
| SoC | Marvell PXA1088, dual-core Cortex-A7 (ARMv7), NEON/VFPv3 |
| GPU | Vivante GC1000 (galcore driver in-tree; stock blobs reverse-engineered — see [docs/12-gpu.md](docs/12-gpu.md)) |
| RAM | 1.5 GB |
| Display | 7" 800×1280, `mmp-fb` framebuffer |
| Touch | Zinitix bt532 (`sec_touchscreen`), capacitive |
| Audio | Marvell 88pm805 PMIC codec, Class-D speaker amplifier |
| Wi-Fi / BT | Marvell SD8887 (mwifiex) |
| Storage | eMMC (`mmcblk0`) |

## Status

| Subsystem | Status | Notes |
|---|---|---|
| Boot image / bootloader | Working | `pxa-mkbootimg`, PXA1088 DTB blob, flashed to `mmcblk0p10` |
| Kernel 3.10.0-t4 | Working | degas3g source + postmarketOS + custom patches |
| Initramfs | Working | busybox, USB-NCM debug shell, `switch_root` |
| Root filesystem | Working | Alpine 3.20.3 armhf, OpenRC, ext4 on `mmcblk0p15` |
| Display | Working | fbdev, framebuffer console |
| Xorg 1.21 | Working | fbdev driver |
| XFCE4 desktop | Working | autostart via `t4desktop` service (startx) |
| Touchscreen | Working | udev + libinput |
| Audio (speaker) | Working | 88pm805 manual codec bring-up |
| Volume keys | Working | gpio-keys → ALSA mixer |
| USB networking | Working | CDC-NCM gadget, SSH (dropbear), telnet |
| Power button | Partial | key events delivered; binding pending |
| Home / Back / Recent | Partial | keycodes mapped; actions not finalized |
| Wi-Fi (SD8887) | Not working | firmware installed; driver bring-up pending |
| GPU acceleration | In progress | galcore 4.6.9.8290 loaded & probed; blobs verified (version-coupling proven); bionic runtime layer working; stock libGAL round-trips real ioctls (HAL construct + chip/memory queries OK) — [docs/12-gpu.md](docs/12-gpu.md) |

## Requirements

* Host: macOS or Linux with Docker, `adb`, `expect`, `python3`, and GNU `nc`.
* An ARM binfmt handler for cross-building armhf packages:
  `docker run --privileged --rm tonistiigi/binfmt --install arm`
* The device in TWRP recovery with `adb` connectivity (used for flashing only).

## Building

The build runs in an x86_64 container and is split into numbered, individually
runnable stages under `build/scripts/`:

```sh
docker build -t t4build build/

docker run --rm -v "$PWD/..":/work -v t4ksrc:/work/repo/kernel-degas t4build \
    bash /work/build/scripts/30-kernel.sh       # zImage, modules, dt.img
docker run --rm -v "$PWD/..":/work t4build bash /work/build/scripts/40-initramfs.sh
docker run --rm -v "$PWD/..":/work t4build bash /work/build/scripts/50-bootimg.sh
docker run --rm -v "$PWD/..":/work t4build bash /work/build/scripts/60-rootfs.sh
```

Outputs are written to `dist/` (`boot-linux-dt.img`, `rootfs.img`, logs).

## Installation

With the device in TWRP:

```sh
BOOTIMG=dist/boot-linux-dt.img bash build/scripts/70-flash.sh          # kernel
BOOTIMG=dist/boot-linux-dt.img bash build/scripts/70-flash.sh rootfs   # root filesystem
```

This writes the boot image to `mmcblk0p10` and the root filesystem to
`mmcblk0p15`, replacing the Android system partition. Both operations verify
the result by reading the partition back. See `docs/06-flashing.md`.

## Usage

After reboot, the device boots to the XFCE4 desktop and brings up a USB
ethernet interface (`usb0`, address `10.42.0.1`):

```sh
ssh root@10.42.0.1          # password: t4
telnet 10.42.0.1 2323       # initramfs shell (recovery channel)
```

Default credentials: `root:t4`, `t4:t4` (the `t4` user has passwordless sudo).

## Repository layout

| Path | Contents |
|---|---|
| `build/` | Dockerfile, numbered pipeline scripts, initramfs skeleton, kernel config |
| `patches/kernel/` | Kernel patches (touch input, postmarketOS gcc/backport set) |
| `tools/` | Host-side helpers for file transfer and device access |
| `rootfs-overlay/` | Files installed into the root filesystem (X11, services, audio) |
| `docs/` | Stage-by-stage documentation |

## Documentation

See [`docs/README.md`](docs/README.md) for the full index. Notable entries:

* `docs/07-desktop-x11.md` — Xorg bring-up: three distinct defects with identical symptoms
* `docs/09-audio.md` — 88pm805 codec register initialization
* `docs/99-debugging.md` — debugging methodology and tooling

## Known limitations

* **GPU acceleration in progress.** The Vivante GC1000 has no open-source
  driver for kernel 3.10, so the stock Android blobs are being ported instead
  (`docs/12-gpu.md`). Kernel driver (galcore 4.6.9.8290) is loaded and the
  kernel↔blob contract is verified; the bionic runtime layer works on Alpine
  and the stock `libGAL.so` already round-trips real ioctls (construct, chip
  identity, video memory). Next milestone: EGL bring-up. Rendering remains
  software until then.
* **Kernel 3.10.** Precludes systemd-based distributions (systemd requires
  ≥4.15). Init systems must be OpenRC, runit, or sysvinit.
* **Wi-Fi** is not yet operational (see `docs/11-networking.md`).
* **Memory.** 1.3 GB usable RAM limits concurrent applications and browsing.

## References

This port builds on prior work; see [`REFERENCES.md`](REFERENCES.md) for the
kernel source, postmarketOS aports (kernel patches and the UCM2 audio
configuration), `pxa-mkbootimg`, Alpine Linux, and busybox.

## License

Kernel patches are derived from GPL-2.0 kernel sources and remain under GPL-2.0.
The scripts, configuration, and documentation in this repository are provided
as-is, without warranty, for use on hardware you own. Upstream projects retain
their respective licenses.
