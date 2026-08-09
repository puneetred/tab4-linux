#!/bin/bash
# Configure the degas3g kernel: degas3g defconfig + pmos patches + linux tweaks
set -e
K=/work/repo/kernel-degas
PATCHDIR=${PATCHDIR:-/work/build/patches/kernel}
cd "$K"
echo "=== [20] apply pmos patches ==="
for p in "$PATCHDIR"/*.patch; do
  n=$(basename "$p")
  if patch -p1 --dry-run -s < "$p" >/dev/null 2>&1; then
    patch -p1 -s < "$p" && echo "APPLIED $n"
  else
    patch -p1 -R --dry-run -s < "$p" >/dev/null 2>&1 && echo "ALREADY $n" || echo "FAILED  $n"
  fi
done
echo "=== [20] synthesize compiler-gccN headers ==="
for v in 5 6 7 8 9 10 11 12 13 14; do
  [ -f include/linux/compiler-gcc$v.h ] || cp include/linux/compiler-gcc4.h include/linux/compiler-gcc$v.h
done
echo "=== [20] fix vsync-wait in atomic context (fbcon+rpm recursion wedge) ==="
VS=drivers/video/mmp/hw/vsync.c
if ! grep -q 'in_atomic()' "$VS"; then
python3 - "$VS" <<'PYEOF'
import sys
p=sys.argv[1]; s=open(p).read()
if 'linux/hardirq.h' not in s:
    s=s.replace('#include <video/mmp_disp.h>', '#include <linux/hardirq.h>\n#include <video/mmp_disp.h>',1)
for fn in ('&path->vsync;','&path->special_vsync;'):
    old='\tstruct mmp_vsync *vsync = %s\n\tatomic_set(&vsync->ready, 0);'%fn
    new='\tstruct mmp_vsync *vsync = %s\n\tif (in_atomic() || irqs_disabled())\n\t\treturn 0;\n\tatomic_set(&vsync->ready, 0);'%fn
    assert old in s, fn
    s=s.replace(old,new,1)
open(p,'w').write(s)
print('patched',p)
PYEOF
fi
grep -q 'in_atomic' "$VS" || { echo "VSPATCH-FAILED"; exit 1; }

echo "=== [20] defconfig ==="
mkdir -p out
make O=out ARCH=arm pxa1088_degas3g_eur_defconfig
C="scripts/config --file out/.config"
$C --set-str LOCALVERSION "-t4"
$C -e IKCONFIG -e IKCONFIG_PROC
$C -e VT -e VT_CONSOLE -e VT_HW_CONSOLE_BINDING -e UNIX98_PTYS
$C -e FRAMEBUFFER_CONSOLE -e FONTS -e FONT_8x8 -e FONT_8x16
$C -e INPUT_EVDEV
$C -e MAGIC_SYSRQ -e KALLSYMS -e DEBUG_FS
$C -e DEVPTS_MULTIPLE_INSTANCES
$C -d ANDROID_PARANOID_NETWORK
$C -d USB_G_ANDROID
$C -e USB_G_NCM
$C -d MRVL_MMP_MODEM

echo "=== [20] fix u_ether/ncm module build (Marvell tree quirk) ==="
UE=drivers/usb/gadget/u_ether.c
if ! grep -q 'U_ETHER_EMBEDDED' "$UE"; then
  sed -i 's|^#include "u_ether.h"|#include "u_ether.h"\n#include "rndis.h"|' "$UE"
  sed -i 's|^module_init(gether_init);|#ifndef U_ETHER_EMBEDDED\nmodule_init(gether_init);\n#endif|' "$UE"
  sed -i 's|^module_exit(gether_exit);|#ifndef U_ETHER_EMBEDDED\nmodule_exit(gether_exit);\n#endif|' "$UE"
  echo "patched $UE"
fi
NC=drivers/usb/gadget/ncm.c
if ! grep -q 'U_ETHER_EMBEDDED' "$NC"; then
  sed -i 's|^#include "u_ether.c"|#define U_ETHER_EMBEDDED 1\n#include "u_ether.c"|' "$NC"
  sed -i 's|^\treturn usb_composite_probe(&ncm_driver);|\tint ret = gether_init();\n\tif (ret)\n\t\treturn ret;\n\treturn usb_composite_probe(\&ncm_driver);|' "$NC"
  sed -i 's|^\tusb_composite_unregister(&ncm_driver);|\tusb_composite_unregister(\&ncm_driver);\n\tgether_exit();|' "$NC"
  echo "patched $NC"
fi
$C -e MMP_PANEL_S6D7AA0X
$C -e FRAMEBUFFER_CONSOLE_DETECT_PRIMARY
$C -e MMC_SDHCI -e MMC_SDHCI_PXAV3
$C -e EXT4_FS -e TMPFS -e DEVTMPFS -e DEVTMPFS_MOUNT
$C -e IKCONFIG_PROC
make O=out ARCH=arm olddefconfig
echo "=== [20] verify key options ==="
grep -E '^CONFIG_(LOCALVERSION|IKCONFIG|FRAMEBUFFER_CONSOLE|INPUT_EVDEV|USB_G_ANDROID|MMC_SDHCI_PXAV3|DEVTMPFS|EXT4_FS|MMP_DISP|FB|VT=|VT_CONSOLE)' out/.config | head -25
echo "=== [20] DONE ==="
