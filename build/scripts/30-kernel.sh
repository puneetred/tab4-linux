#!/bin/bash
# Build kernel zImage + modules + PXA-DT blob
set -e
K=/work/repo/kernel-degas
cd "$K"
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export KCFLAGS="-march=armv7-a"
N=$(nproc)
echo "=== [30] zImage (-j$N) ==="
make O=out -j$N zImage > /work/dist/kbuild-zImage.log 2>&1 || { tail -40 /work/dist/kbuild-zImage.log; exit 1; }
tail -3 /work/dist/kbuild-zImage.log
echo "=== [30] modules ==="
make O=out -j$N CFLAGS_MODULE="-fno-pic -Wno-error" modules > /work/dist/kbuild-modules.log 2>&1 || { tail -40 /work/dist/kbuild-modules.log; exit 1; }
tail -3 /work/dist/kbuild-modules.log
echo "=== [30] dtbs ==="
make O=out -j$N dtbs > /work/dist/kbuild-dtbs.log 2>&1 || { tail -40 /work/dist/kbuild-dtbs.log; exit 1; }
tail -3 /work/dist/kbuild-dtbs.log
ls -la out/arch/arm/boot/zImage
echo "=== [30] PXA-DT blob ==="
# dtbTool execs "<dtc_path>dtc -I dtb -O dts" -> -p MUST end with '/' and be relative to $K
/work/dist/bin/pxa1088-dtbTool -p out/scripts/dtc/ -o /work/dist/dt.img out/arch/arm/boot/dts/ 2>&1 | tail -15
ls -la /work/dist/dt.img && xxd /work/dist/dt.img | head -3
echo "=== [30] install modules ==="
make O=out modules_install INSTALL_MOD_PATH=/work/dist/mod 2>&1 | tail -3
find /work/dist/mod -name '*.ko' | head -20
echo "=== [30] DONE ==="
