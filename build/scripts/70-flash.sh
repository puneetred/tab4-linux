#!/bin/bash
# Stage 6: flash boot image (+optional rootfs) from macOS host via TWRP adb
set -e
cd /Users/puneetjain/Documents/tab4
BOOTIMG=${BOOTIMG:-dist/boot-linux.img}
adb wait-for-device
echo "[*] device: $(adb shell 'getprop ro.product.device' | tr -d '\r')"
echo "[*] sanity: boot image header"
xxd "$BOOTIMG" | head -2
echo "[*] push boot image"
adb push "$BOOTIMG" /data/boot-linux.img
echo "[*] flash KERNEL (mmcblk0p10)"
adb shell "dd if=/data/boot-linux.img of=/dev/block/mmcblk0p10 bs=1M 2>/dev/null && sync"
SZ=$(stat -f %z "$BOOTIMG")
echo "[*] verify readback ($SZ bytes)"
adb exec-out "dd if=/dev/block/mmcblk0p10 bs=1M count=$(( (SZ+1048575)/1048576 )) 2>/dev/null" | head -c "$SZ" > /tmp/readback.img
cmp /tmp/readback.img "$BOOTIMG" && echo "[OK] boot image verified" || { echo "[FAIL] mismatch"; exit 1; }
adb shell "rm /data/boot-linux.img"

if [ "$1" = "rootfs" ]; then
  echo "[*] compress rootfs"
  test -f dist/rootfs.img.gz || gzip -9 -k dist/rootfs.img
  echo "[*] push rootfs ($(ls -la dist/rootfs.img.gz | awk '{print $5}') bytes)"
  adb push dist/rootfs.img.gz /data/rootfs.img.gz
  echo "[*] flash SYSTEM (mmcblk0p15) - this wipes Android system (already broken)"
  adb shell "gzip -dc /data/rootfs.img.gz | dd of=/dev/block/mmcblk0p15 bs=4M 2>/dev/null && sync"
  adb shell "rm /data/rootfs.img.gz"
  echo "[OK] rootfs flashed"
fi
echo "[*] done. Reboot with: adb reboot"
