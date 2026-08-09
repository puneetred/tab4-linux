# 06 — Flashing (TWRP + adb)

Device must be in **TWRP** with `adb` up. `build/scripts/70-flash.sh`.

> **Destructive.** Writes KERNEL (p10) and, with the `rootfs` argument, SYSTEM
> (p15). Use only on a device you are prepared to erase. The stock partitions
> can be restored from the `backup/` dump.

## Kernel

```sh
BOOTIMG=dist/boot-linux-dt.img bash build/scripts/70-flash.sh
```

What it does: `adb push` the image to `/data`, `dd` it to `/dev/block/mmcblk0p10`,
then **read the partition back and `cmp`** against the source. Mismatch = fail.

## Rootfs

```sh
BOOTIMG=dist/boot-linux-dt.img bash build/scripts/70-flash.sh rootfs
```

gzips `dist/rootfs.img`, pushes, and `gzip -dc | dd of=/dev/block/mmcblk0p15`.

## On-device re-flash (no adb, after Linux is up)

Once booted you can update the kernel from inside Linux over the USB link:

```sh
# host serves, tab fetches (busybox nc: Mac → tab direction is the reliable one)
nc -l 5555 < dist/boot-linux-dt.img            # on the Mac (via screen)
# on the tab:
nc 10.42.0.100 5555 > /root/boot-new.img
dd if=/root/boot-new.img of=/dev/mmcblk0p10 bs=4M && sync
reboot -f
```

`tools/t4push.py` handles the Mac→tab direction robustly (orderly FIN + drain).

## Gotchas

* **`/dev/block/mmcblk0p15` vs `/dev/mmcblk0p15`**: under our devtmpfs the
  `/dev/block/...` symlink dir doesn't exist — `dd` to a nonexistent path
  *silently no-ops*. Use `/dev/mmcblk0pNN` directly and always verify by
  mounting/reading back. (This cost us a "phantom successful flash".)
* After flashing rootfs, **verify before reboot**: `mount -o ro /dev/mmcblk0p15 /x`
  and check `/x/etc/t4-rootfs` exists.
