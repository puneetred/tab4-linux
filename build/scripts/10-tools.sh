#!/bin/bash
# Build host-side tools: pxa-mkbootimg suite + static arm busybox
set -e
cd /work
mkdir -p dist/bin src
echo "=== [10] pxa-mkbootimg suite ==="
if [ ! -d src/pxa-mkbootimg ]; then
  curl -sL -o src/pxa-mkbootimg.tgz https://github.com/osm0sis/pxa-mkbootimg/archive/2020.05.19.tar.gz
  tar -C src -xzf src/pxa-mkbootimg.tgz
  mv src/pxa-mkbootimg-2020.05.19 src/pxa-mkbootimg
fi
make -C src/pxa-mkbootimg
install -m755 src/pxa-mkbootimg/pxa-mkbootimg dist/bin/
install -m755 src/pxa-mkbootimg/pxa-unpackbootimg dist/bin/
install -m755 src/pxa-mkbootimg/pxa1088-dtbTool dist/bin/
install -m755 src/pxa-mkbootimg/pxa1908-dtbTool dist/bin/
echo "=== [10] busybox static arm ==="
if [ ! -d src/busybox-1.36.1 ]; then
  curl -sL -o src/busybox.tar.bz2 https://busybox.net/downloads/busybox-1.36.1.tar.bz2
  tar -C src -xjf src/busybox.tar.bz2
fi
cd src/busybox-1.36.1
make defconfig
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
make -j"$(nproc)" ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- busybox
install -m755 busybox /work/dist/bin/busybox-arm
file /work/dist/bin/busybox-arm
echo "=== [10] DONE ==="
ls -la /work/dist/bin/
