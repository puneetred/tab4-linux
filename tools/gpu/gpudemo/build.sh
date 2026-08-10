#!/bin/sh
# Build the bionic-linked animated GPU demo (gpudemo).
# Links against the ROM's own libc.so/libm.so/libdl.so; runs on-device
# with LD_LIBRARY_PATH=/system/lib (see docs/12-gpu.md stage 5).
# Usage: build.sh <stage2-lib-dir>
set -e
cd "$(dirname "$0")"
BIONIC="${1:-/Users/puneetjain/Documents/tab4/rom/stage2/lib}"
docker run --rm -v "$PWD":/work -v "$BIONIC":/bionic t4build bash -c '
    cd /work
    arm-linux-gnueabihf-gcc -c gpudemo.c -o gpudemo.o -O2 -marm -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
    arm-linux-gnueabihf-gcc -c crte.S -o crte.o
    arm-linux-gnueabihf-ld -o gpudemo -e _start \
        --dynamic-linker /system/bin/linker -rpath /system/lib \
        -L/bionic crte.o gpudemo.o -lc -ldl -lm
    arm-linux-gnueabihf-size gpudemo
    echo "--- interpreter ---"
    arm-linux-gnueabihf-readelf -l gpudemo | grep -i interpreter
'
