#!/bin/sh
# Build the bionic-linked EGL/GLES2 offscreen render test.
# Links against the ROM's own libc.so/libdl.so; runs on-device with
# LD_LIBRARY_PATH=/system/lib (see docs/12-gpu.md stage 4).
# Usage: build.sh <stage2-lib-dir>
set -e
cd "$(dirname "$0")"
BIONIC="${1:-/Users/puneetjain/Documents/tab4/rom/stage2/lib}"
docker run --rm -v "$PWD":/work -v "$BIONIC":/bionic t4build bash -c '
    cd /work
    arm-linux-gnueabihf-gcc -c egl-test.c -o egl-test.o -O2 -marm -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
    arm-linux-gnueabihf-gcc -c crte.S -o crte.o
    arm-linux-gnueabihf-ld -o egl-test -e _start \
        --dynamic-linker /system/bin/linker -rpath /system/lib \
        -L/bionic crte.o egl-test.o -lc -ldl
    arm-linux-gnueabihf-size egl-test
    echo "--- interpreter ---"
    arm-linux-gnueabihf-readelf -l egl-test | grep -i interpreter
'
