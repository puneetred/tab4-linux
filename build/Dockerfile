FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture i386 \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    build-essential gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
    bc bison flex libssl-dev libncurses-dev cpio lzop lz4 xz-utils gzip bzip2 \
    python3 git curl ca-certificates kmod device-tree-compiler u-boot-tools \
    libc6-i386 qemu-user-static e2fsprogs file rsync sudo vim-tiny \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
CMD ["sleep", "infinity"]
