# 05 — Rootfs (Alpine 3.20.3 armhf + XFCE4)

Built by `build/scripts/60-rootfs.sh` (runs in the x86_64 container under
qemu-user chroot), packed by `65-pack-rootfs.sh`.

## Base

```sh
tar -xzf alpine-minirootfs-3.20.3-armhf.tar.gz -C rootfs/
cp /usr/bin/qemu-arm-static rootfs/usr/bin/
chroot rootfs /usr/bin/qemu-arm-static /bin/sh -c 'apk add ...'
```

Packages (the interesting ones): `alpine-base openrc xorg-server
xf86-video-fbdev xf86-input-evdev xf86-input-libinput xfce4 xfce4-terminal
mousepad lightdm dbus eudev wpa_supplicant dhcpcd dropbear alsa-utils
e2fsprogs util-linux fbgrab onboard ...`

## Customizations baked in

* **`/etc/t4-rootfs`** marker (initramfs only switches into marked rootfs).
* **users**: `root:t4`, `t4:t4` (wheel, video, input, audio groups; sudo NOPASSWD).
* **`t4desktop` OpenRC service** → `startx :0 vt07` → `/root/.xinitrc` = `startxfce4`.
  Display manager intentionally bypassed (see docs/07).
* **`xfwm4.xml`**: `use_compositing=false` (no GL provider → compositor blanks the screen).
* **pure-C libpixman** swapped in (see docs/07).
* **Xorg config** `rootfs-overlay/etc/X11/xorg.conf.d/10-t4.conf` (fbdev + screen only;
  input is auto-configured via udev+libinput).
* **local.d**: `t4sound.start` (codec bring-up), `sd8887.start` (wifi power),
  `t4-firstboot.start` (resize rootfs + usb0 dhcp).
* **kernel modules** for `3.10.0-t4` + Marvell firmware (`mrvl/*`).
* **`/etc/modules`**: `mlan`, `sd8887`, `galcore`.

## Packing (65-pack-rootfs.sh)

* ext4, **but** the 3.10 kernel can't mount `64bit`/`metadata_csum`, so:
  `mkfs.ext4 -O '^64bit,^metadata_csum,^metadata_csum_seed'`.
* Copy via `tar` through a loop mount (avoids bind-mount xattr issues in Docker).

## Why Alpine (and its ceiling)

musl + OpenRC runs on 3.10 and is tiny. The trade-off is fewer prebuilt
creature comforts than Debian. If you want apt/Firefox/LibreOffice, see
docs/11 "next distros" — the boot/kernel/initramfs we built are distro-agnostic;
only the rootfs payload changes.
