#!/bin/sh
# Build the bionic-linked libGAL round-trip test.
# Links against the ROM's own libc.so/libdl.so; runs on-device with
# LD_LIBRARY_PATH=/system/lib (see docs/12-gpu.md stage 3).
# Usage: build.sh <stage2-lib-dir>
set -e
cd "$(dirname "$0")"
BIONIC="${1:-/Users/puneetjain/Documents/tab4/rom/stage2/lib}"
docker run --rm -v "$PWD":/work -v "$BIONIC":/bionic t4build bash -c '
    cd /work
    arm-linux-gnueabihf-gcc -c galcore-test.c -o galcore-test.o -O2 -marm -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
    arm-linux-gnueabihf-gcc -c crte.S -o crte.o
    arm-linux-gnueabihf-ld -o galcore-test -e _start \
        --dynamic-linker /system/bin/linker -rpath /system/lib \
        -L/bionic crte.o galcore-test.o -lc -ldl
    arm-linux-gnueabihf-size galcore-test
    echo "--- interpreter ---"
    arm-linux-gnueabihf-readelf -l galcore-test | grep -i interpreter
'
