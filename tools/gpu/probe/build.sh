#!/bin/sh
# Build the galcore kernel-contract probe for the device.
# Usage: build.sh   (outputs src/galcore-probe, static armhf)
set -e
cd "$(dirname "$0")"
docker run --rm -v "$PWD/../..":/work -v "$PWD":/probe t4build bash -c '
    cd /probe/src
    arm-linux-gnueabihf-gcc -static -I../inc -o galcore-probe galcore-probe.c
    arm-linux-gnueabihf-size galcore-probe
'
