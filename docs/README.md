# Documentation index

Read in this order for the full build, or jump to what you need.

| Doc | Stage |
|---|---|
| [00-overview.md](00-overview.md) | Architecture, boot chain, key decisions |
| [01-device.md](01-device.md) | Hardware, partitions, recon, full backup |
| [02-kernel.md](02-kernel.md) | Kernel source, config, patches, DTB |
| [03-bootimage.md](03-bootimage.md) | pxa-mkbootimg, header, verify |
| [04-initramfs.md](04-initramfs.md) | busybox init, USB-NCM debug net, switch_root |
| [05-rootfs.md](05-rootfs.md) | Alpine XFCE rootfs build + pack |
| [06-flashing.md](06-flashing.md) | adb/TWRP flash, readback, on-device reflash |
| [07-desktop-x11.md](07-desktop-x11.md) | **The 3 Xorg bugs** (musl a_crash, DefaultDepth16, compositor) |
| [08-touchscreen.md](08-touchscreen.md) | bt532, evdev patch, libinput+udev |
| [09-audio.md](09-audio.md) | 88pm805 manual codec bring-up, volume |
| [10-buttons.md](10-buttons.md) | power/vol/home/back keycodes + bindings |
| [11-networking.md](11-networking.md) | USB-NCM, ssh, package plumbing, WiFi TODO, next distros |
| [12-gpu.md](12-gpu.md) | **GPU port**: kernel contract probe, stock-blob extraction, stages 0–6 |
| [99-debugging.md](99-debugging.md) | Methodology: core dumps, NT_FILE, regmap, nc plumbing |

## Common pitfalls

1. `/dev/block/mmcblk0p15` doesn't exist under devtmpfs → `dd` silently no-ops.
   Use `/dev/mmcblk0p15` and **read back**.
2. Xorg SIGILL at `...674` = musl `a_crash()` from the udev-probe path, **not** NEON.
3. `DefaultDepth 16` SIGILLs — stay at native 32bpp.
4. xfwm4 compositor blanks the screen (no GL) → `use_compositing=false`.
5. libinput needs **eudev** (`udev device never initialized` otherwise); evdev
   on the MT stream gives touch-motion but **zero clicks**.
6. `xfce4-power-manager` swallows the power key (calls absent logind).
7. 88pm805 codec has **no DAPM** — the whole output path is powered off until you
   poke registers (pmOS UCM2 values). Its readback is unreliable → track volume
   in a file.
8. systemd distros won't boot on kernel 3.10 — use OpenRC/runit/sysvinit.
