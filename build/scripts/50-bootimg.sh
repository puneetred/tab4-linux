#!/bin/bash
# Pack boot image(s) with pxa-mkbootimg and verify against TWRP header layout
set -e
cd /work
K=out-placeholder
Z=/work/repo/kernel-degas/out/arch/arm/boot/zImage
R=/work/dist/initramfs.cpio.gz
MB=/work/dist/bin/pxa-mkbootimg
UN=/work/dist/bin/pxa-unpackbootimg

pack() { # $1 = dt blob, $2 = out name
  "$MB" --kernel "$Z" --ramdisk "$R" --dt "$1" \
    --base 0x10000000 --ramdisk_offset 0x01000000 --pagesize 2048 \
    -o "/work/dist/$2"
  echo "--- $2 ---"; ls -la "/work/dist/$2"
}

# Variant A: TWRP's own PXA-DT blob (proven on this exact unit)
pack /work/build/twrp-pxa-dt.bin boot-linux.img
# Variant B: freshly generated dt.img from kernel dts (if it exists)
[ -f /work/dist/dt.img ] && pack /work/dist/dt.img boot-linux-dt.img

echo "=== verify: unpack our image ==="
rm -rf /tmp/vfy && mkdir -p /tmp/vfy
"$UN" -i /work/dist/boot-linux.img -o /tmp/vfy | head -20

echo "=== header compare: ours vs TWRP ==="
python3 - <<'EOF'
import struct
def hdr(p):
    d=open(p,'rb').read(64)
    m=d[0:8]; ks,ka,rs,ra,ss,sa,ts,ps=struct.unpack('<8I',d[8:40])
    return m,ks,ka,rs,ra,ss,sa,ts,ps,struct.unpack('<I',d[40:44])[0]
for f in ['/work/dist/boot-linux.img','/work/backup/mmcblk0p9.img']:
    m,ks,ka,rs,ra,ss,sa,ts,ps,d1=hdr(f)
    print(f, m, hex(ks),hex(ka),hex(rs),hex(ra),hex(ss),hex(sa),hex(ts),hex(ps),hex(d1))
EOF
echo "=== [50] DONE ==="
