# 01 — Device: SM-T231 (degas3g)

## Hardware

| Part | Detail |
|---|---|
| SoC | Marvell **PXA1088** (2× Cortex-A7 @ ~1.2 GHz, ARMv7, NEON/VFPv3) |
| GPU | Vivante **GC1000** (no FOSS driver on 3.10 → unused) |
| RAM | 1.5 GB (≈1288 MB visible) |
| Panel | 7" 800×1280, `mmp-fb` framebuffer, `lcd_id=0x005eb810` |
| Touch | Zinitix **bt532** (`sec_touchscreen`, /dev/input/event0) — capacitive, also carries Back/Recent keys |
| Audio | Marvell **88pm805** PMIC codec (`pxa-88pm805-dkb-hifi` ALSA card), Class-D speaker amp |
| WiFi/BT | Marvell **SD8887** (mwifiex), firmware `mrvl/sd8887_uapsta.bin` |
| PMIC keys | `88pm80x_on` (power), `gpio-keys` (vol+/vol−/home) |
| Storage | eMMC `mmcblk0`, ~8 GB |

## Partition map (read live in TWRP)

The important ones:

| Part | Name | Use here |
|---|---|---|
| `mmcblk0p9` | (stock boot, reference) | header template for pxa-mkbootimg |
| `mmcblk0p10` | **KERNEL** | we flash `boot-linux-dt.img` here |
| `mmcblk0p15` | **SYSTEM** | we flash the Linux rootfs (ext4) here — *wipes Android system* |

The rest (param, cache, userdata, modem, etc.) are left alone.

## Getting facts off the device (TWRP + adb)

```sh
adb shell 'cat /proc/partitions'
adb shell 'for d in /dev/block/platform/*/by-name; do ls -l $d; done'
adb shell 'cat /proc/cmdline'          # base addresses, lcd_id, board_id
adb shell 'zcat /proc/config.gz'       # stock kernel config
adb shell 'getprop ro.product.device; getprop ro.bootloader'
```

## Full backup (do this first)

```sh
mkdir -p backup
adb shell 'cat /proc/partitions'        # pick the pN list
for p in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
  adb exec-out "dd if=/dev/block/mmcblk0p$p 2>/dev/null" > backup/mmcblk0p$p.img
done
# big ones (system/userdata) take a while; verify sizes after.
```

The stock **kernel cmdline** observed (drives the display/touch selections):

```
initrd=0x01400000,12m rw uart_dma vmalloc=0x10000000 hwdfc=1 qhd_lcd=1 \
touch_type=0 androidboot.hardware=pxa1088 lcd_id=0x005eb810 board_id=0x03 \
max_freq=1183 disp_start_addr=0x17000000 androidboot.lcd=WVGA ...
```

`board_id=0x03` selects the `pxa1088-degas3g-r03` DTB in the blob.
