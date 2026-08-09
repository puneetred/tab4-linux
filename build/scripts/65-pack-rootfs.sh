#!/bin/bash
# Pack /work/rootfs into dist/rootfs.img (loop-mount + tar; avoids bind-mount xattr issues)
set -e
cd /work
ROOT=/work/rootfs
echo "=== [65] pack rootfs -> ext4 image ==="
rm -f /work/dist/rootfs.img
truncate -s 1700M /tmp/rootfs.img
# 3.10 kernel: no 64bit/metadata_csum/etc.
mkfs.ext4 -q -L t4root -b 4096 -O '^64bit,^metadata_csum,^metadata_csum_seed' /tmp/rootfs.img
mkdir -p /tmp/rmnt
mount -o loop /tmp/rootfs.img /tmp/rmnt
( cd "$ROOT" && tar cf - --exclude=./proc --exclude=./sys --exclude=./dev . ) | tar xf - -C /tmp/rmnt
mkdir -p /tmp/rmnt/proc /tmp/rmnt/sys /tmp/rmnt/dev
sync
umount /tmp/rmnt
mv /tmp/rootfs.img /work/dist/rootfs.img
ls -la /work/dist/rootfs.img
echo "=== [65] DONE ==="
