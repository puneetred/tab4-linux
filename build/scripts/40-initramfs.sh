#!/bin/bash
# Assemble initramfs (busybox + init) -> dist/initramfs.cpio.gz
set -e
IR=/tmp/initramfs
rm -rf "$IR"
cp -r /work/build/initramfs-skel "$IR"
install -m755 /work/dist/bin/busybox-arm "$IR"/bin/busybox
# gadget modules for usb networking (only if built as modules)
mkdir -p "$IR"/lib
for m in g_ncm.ko g_cdc.ko; do
  find /work/dist/mod -name "$m" -exec install -m644 {} "$IR"/lib/ \; 2>/dev/null
done
# static console/null (container-local fs: mknod works here; devtmpfs covers the rest)
mknod "$IR"/dev/console c 5 1
mknod "$IR"/dev/null c 1 3
chmod 755 "$IR"/init
cd "$IR"
find . | cpio -o -H newc --owner 0:0 2>/dev/null | gzip -9 > /work/dist/initramfs.cpio.gz
ls -la /work/dist/initramfs.cpio.gz
echo "=== [40] DONE ==="
