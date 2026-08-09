# 03 — Boot image (pxa-mkbootimg)

The PXA1088 uses a standard `ANDROID!` boot header **plus** a Marvell DTB blob
appended. Stock `mkbootimg` won't build the DTB part; we use
**osm0sis/pxa-mkbootimg** (built in build/scripts/10-tools.sh).

## Packing (build/scripts/50-bootimg.sh)

```sh
pxa-mkbootimg \
  --kernel  zImage \
  --ramdisk initramfs.cpio.gz \
  --dt      dt.img \
  --base 0x10000000 \
  --ramdisk_offset 0x01000000 \
  --pagesize 2048 \
  -o boot-linux-dt.img
```

These numbers came straight off the device's cmdline/boot header:

* `--base 0x10000000`, `--ramdisk_offset 0x01000000` (initrd load `0x01400000`).
* `--pagesize 2048` (matches the stock header; verify against `backup/mmcblk0p9.img`).

## Two DTB sources

* `build/twrp-pxa-dt.bin` — the blob TWRP itself uses (known-good on this unit).
* `dist/dt.img` — regenerated from the kernel DTS by `pxa1088-dtbTool`.

Both produce a bootable image; we flash the freshly generated one
(`boot-linux-dt.img`).

## Verify before flashing (50-bootimg.sh does this)

```sh
pxa-unpackbootimg -i boot-linux-dt.img -o /tmp/vfy
# and a python header diff vs the stock backup: magic, kernel/ramdisk sizes,
# addresses, second/tags, pagesize must line up.
```

## Flash target

`mmcblk0p10` (KERNEL). The flash script reads the partition back and `cmp`s it
against the source so a silent short-write can't slip through.
