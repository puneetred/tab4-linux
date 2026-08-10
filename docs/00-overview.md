# 00 — Architecture & boot chain

## Goals / non-goals

* **Goal:** a *real* Linux environment with a desktop, booted from the eMMC,
  using the stock bootloader. No Android runtime, no chroot-on-Android, no
  TWRP-as-init.
* **Non-goal:** was GPU/3D — but the stock Vivante blobs turned out to be
  portable (see [12-gpu.md](12-gpu.md)): **GPU acceleration now works on
  Alpine** (stages 0–5). Before that was proven, everything was
  software-rendered on the fbdev framebuffer.

## Boot chain (what actually happens on power-on)

```
PBL (boot ROM in PXA1088)
   │  loads from eMMC
   ▼
SBL / Samsung secondary bootloader (mmcblk0p* boot partitions)
   │  reads the ANDROID! boot image header
   ▼
Linux kernel 3.10.0-t4  +  initramfs        ← our boot-linux-dt.img on mmcblk0p10
   │  DTB blob (pxa1088-degas3g-r0X.dtb via pxa1088-dtbTool) selects board
   │  init = busybox script:
   │    1. mount devtmpfs/proc/sys
   │    2. bring up USB-NCM gadget  → usb0 10.42.0.1, telnetd :2323  (survives switch_root)
   │    3. wait for /dev/mmcblk0p15
   │    4. mount SYSTEM (ext4), check /etc/t4-rootfs marker
   ▼
switch_root → Alpine 3.20.3 /sbin/init (OpenRC)   ← rootfs.img on mmcblk0p15
   │  runlevel default: dbus, dropbear, local, t4desktop
   │  local.d: t4sound.start (codec bring-up), sd8887.start (wifi pwr), firstboot resize
   ▼
t4desktop service → startx :0 vt07 → Xorg (fbdev) → startxfce4
```

## Why these choices

| Decision | Reason |
|---|---|
| **Alpine armhf** (musl) | tiny, OpenRC (no systemd → runs on 3.10), busybox-friendly. |
| **OpenRC everywhere** | systemd ≥248 needs kernel ≥4.15; we're on 3.10. OpenRC/runit/sysvinit are the only options. |
| **startx, not a DM** | lightdm's autologin needs AccountsService/logind (absent). startx is deterministic. |
| **fbdev X driver** | no DRM/KMS driver for the mmp display on this stack. |
| **libinput via eudev** | Xorg's udev *probing* path SIGILLs (musl ld.so a_crash); tagged devices via udev + libinput catchall is stable. |
| **USB-NCM gadget** | macOS has a native NCM driver → instant `usb0` link with zero host setup. RNDIS also present as fallback. |

## One-time host prep (macOS)

* Docker Desktop (for the x86_64 build container + qemu binfmt for arm).
* `adb` (platform-tools) — used only while the device is in TWRP.
* `expect`, `python3`, GNU `nc` on the Mac.
* Binfmt for arm containers: `docker run --privileged --rm tonistiigi/binfmt --install arm`
  (lets us run `arm32v7/alpine` to rebuild armhf libs like libpixman).

## Directory map of a working tree (not the repo)

```
tab4/
  backup/            raw eMMC partition dumps (p1..p15) — 2+GB, gitignored
  build/             ← this repo's build/ + scripts + initramfs + kernel.config
  repo/kernel-degas/ kernel source (docker volume t4ksrc)
  repo/pmaports/     postmarketOS aports (source of audio/wifi/config)
  rootfs/            unpacked Alpine rootfs being edited
  dist/              build outputs: boot-linux-dt.img, rootfs.img, *.log
```
