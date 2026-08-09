# 02 — Kernel (3.10.0-t4)

## Source

* Base: `Sabsabsab/android_kernel_samsung_degas3g` (Samsung degas3g kernel, 3.10).
* Patches: postmarketOS `linux-samsung-degaswifi` aport (gcc-7/8/10 fixes,
  `fix-assembler-junk`, memfd-seals backport, ...), plus our own.
* Built with `arm-linux-gnueabihf-` in the x86_64 container.

## Build (build/scripts/20-config.sh + 30-kernel.sh)

```sh
make O=out ARCH=arm pxa1088_degas3g_eur_defconfig
# then flip options with scripts/config (see 20-config.sh), then:
make O=out -j$(nproc) zImage modules dtbs
/work/dist/bin/pxa1088-dtbTool -p out/scripts/dtc/ -o dist/dt.img out/arch/arm/boot/dts/
make O=out modules_install INSTALL_MOD_PATH=dist/mod
```

## Config deltas that matter (vs defconfig)

```
CONFIG_LOCALVERSION="-t4"
CONFIG_IKCONFIG=y  CONFIG_IKCONFIG_PROC=y        # /proc/config.gz on device
CONFIG_VT/VT_CONSOLE/FRAMEBUFFER_CONSOLE/FONTS   # text console on the panel
CONFIG_INPUT_EVDEV=y
CONFIG_DEVPTS_MULTIPLE_INSTANCES=y               # ssh/terminal ptys
# CONFIG_ANDROID_PARANOID_NETWORK is not set     # let non-root use net
# CONFIG_USB_G_ANDROID is not set                # drop Android gadget
CONFIG_USB_G_NCM=m                               # CDC-NCM ethernet gadget (usb0)
CONFIG_MMC_SDHCI_PXAV3=y  CONFIG_EXT4_FS=y
CONFIG_DEVTMPFS=y  CONFIG_DEVTMPFS_MOUNT=y
```

## Tree patches applied by 20-config.sh

1. **vsync in_atomic guard** (`drivers/video/mmp/hw/vsync.c`): the fbcon +
   rpm path was recursing/wedging in atomic context during boot; bail out of
   the wait when `in_atomic() || irqs_disabled()`. (Symptom was a repeating
   `bus_add_driver` / WARN stack trace on the panel during early boot.)
2. **USB gadget module glue** (`u_ether.c`, `ncm.c`): the Marvell tree's
   `u_ether` assumes a builtin; added an `U_ETHER_EMBEDDED` guard +
   `rndis.h` include so `g_ncm.ko` builds/loads as a module.
3. **compiler-gccN.h** headers synthesized for gcc 5–14 (3.10 only ships ≤gcc4).

## Touchscreen patch (patches/kernel/bt532-x11-classic-axes.patch)

`bt532_ts.c` only emitted multitouch axes. We mirror finger-0 onto classic
`ABS_X/ABS_Y` + `BTN_TOUCH` so Xorg/evdev can drive the pointer. (The final
setup actually uses **libinput**, which reads the MT stream directly — but the
patch is what made touch work under evdev first. See docs/08.)

## Verifying the right DTB is used

The PXA1088 packs *all* board DTBs into one blob; the bootloader picks by
`board_id`. Ours is `board_id=0x03` → `pxa1088-degas3g-r03.dtb`.
`pxa1088-dtbTool` must be invoked with `-p` ending in `/` and relative to the
kernel dir (it execs `$p/dtc`).

## Gotchas

* **`-fno-pic` for modules**: needed on this old tree to avoid reloc issues.
* **gcc version**: modern gcc chokes on 3.10 without the pmOS patch set.
* Keep `CONFIG_ANDROID_PARANOID_NETWORK` off or dropbear/ssh won't bind.
