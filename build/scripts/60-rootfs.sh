#!/bin/bash
# Stage 5: build Alpine armhf XFCE rootfs (qemu-chroot, no binfmt dependency)
# Runs inside t4build (x86_64) container. Output: /work/dist/rootfs.img
set -e
set -x
cd /work
ALP=3.20
ROOT=/work/rootfs
echo "=== [60] fetch alpine minirootfs ==="
if [ ! -f /work/dist/.apk-done ]; then
if [ ! -f src/alpine-minirootfs-armhf.tar.gz ]; then
  curl -sL -o src/alpine-minirootfs-armhf.tar.gz \
    https://dl-cdn.alpinelinux.org/alpine/v${ALP}/releases/armhf/alpine-minirootfs-3.20.3-armhf.tar.gz
fi
rm -rf "$ROOT"; mkdir -p "$ROOT"
tar -xzf src/alpine-minirootfs-armhf.tar.gz -C "$ROOT"
cp /usr/bin/qemu-arm-static "$ROOT"/usr/bin/
printf 'nameserver 8.8.8.8\nnameserver 1.1.1.1\n' > "$ROOT"/etc/resolv.conf
touch "$ROOT"/etc/t4-rootfs   # marker: initramfs only switch_root into marked rootfs

runroot() { chroot "$ROOT" /usr/bin/qemu-arm-static /bin/sh -c "$*"; }

echo "=== [60] apk: base + xorg + xfce ==="
runroot "apk update || true"
runroot "apk add alpine-base openrc"
runroot "apk add xorg-server xf86-video-fbdev xf86-input-evdev xf86-input-libinput \
  xfce4 xfce4-terminal mousepad lightdm lightdm-gtk-greeter \
  dbus eudev wpa_supplicant dhcpcd dropbear \
  sudo shadow bash nano htop ttf-dejavu adwaita-icon-theme \
  alsa-utils e2fsprogs e2fsprogs-extra util-linux fbgrab setxkbmap xrandr onboard" 2>&1 | tail -3
touch /work/dist/.apk-done
else
runroot() { chroot "$ROOT" /usr/bin/qemu-arm-static /bin/sh -c "$*"; }
cp /usr/bin/qemu-arm-static "$ROOT"/usr/bin/ 2>/dev/null || true
echo "=== [60] apk phase skipped (cached) ==="
fi

echo "=== [60] users ==="
runroot "echo 'root:t4' | chpasswd"
runroot "id t4 >/dev/null 2>&1 || adduser -D -s /bin/bash t4; echo 't4:t4' | chpasswd"
runroot "addgroup t4 wheel; addgroup t4 video; addgroup t4 input; addgroup t4 audio"
runroot "mkdir -p /etc/sudoers.d; echo '%wheel ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/wheel"

echo "=== [60] t4desktop service (startx->XFCE, bypasses DM) ==="
cat > "$ROOT"/etc/init.d/t4desktop <<'EOF'
#!/sbin/openrc-run
description="T4 desktop autostart"
depend() {
	need dbus local
	after firewall
}
start() {
	ebegin "Starting T4 desktop"
	start-stop-daemon --start --background --make-pidfile --pidfile /run/t4x.pid --exec /usr/bin/startx -- :0 vt07
	eend $?
}
stop() {
	ebegin "Stopping T4 desktop"
	pkill Xorg
	eend 0
}
EOF
chmod +x "$ROOT"/etc/init.d/t4desktop
printf '#!/bin/sh\nexec startxfce4\n' > "$ROOT"/root/.xinitrc
chmod +x "$ROOT"/root/.xinitrc
# xfwm4 compositor MUST stay off: no GL provider on this stack (AIGLX/swrast absent)
mkdir -p "$ROOT"/root/.config/xfce4/xfconf/xfce-perchannel-xml
cat > "$ROOT"/root/.config/xfce4/xfconf/xfce-perchannel-xml/xfwm4.xml <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<channel name="xfwm4" version="1.0">
  <property name="general" type="empty">
    <property name="use_compositing" type="bool" value="false"/>
    <property name="vblank_mode" type="string" value="off"/>
    <property name="sync_to_vblank" type="bool" value="false"/>
  </property>
</channel>
EOF
# pure-C libpixman: stock musl pixman ARM-asm paths SIGILL on PXA1088
if [ -f /work/dist/libpixman-nosimd.so ]; then
  cp /work/dist/libpixman-nosimd.so "$ROOT"/usr/lib/libpixman-1.so.0.43.2
  echo "[60] installed pure-C libpixman"
fi

echo "=== [60] lightdm autologin (kept for reference; NOT enabled) ==="
mkdir -p "$ROOT"/etc/lightdm
cat > "$ROOT"/etc/lightdm/lightdm.conf <<'EOF'
[Seat:*]
greeter-session=lightdm-gtk-greeter
user-session=xfce
autologin-user=t4
autologin-user-timeout=0
autologin-session=xfce
EOF
runroot "addgroup -S autologin 2>/dev/null || addgroup autologin; addgroup t4 autologin; addgroup -S nopasswdlogin 2>/dev/null || addgroup nopasswdlogin; addgroup t4 nopasswdlogin"

echo "=== [60] services ==="
runroot "rc-update add devfs sysinit; rc-update add dmesg sysinit; rc-update add mdev sysinit"
runroot "rc-update add modules boot; rc-update add sysctl boot; rc-update add hostname boot; rc-update add bootmisc boot; rc-update add syslog boot"
runroot "rc-update add dbus default; rc-update add t4desktop default; rc-update add dropbear default; rc-update add local default"
runroot "rc-update add udev sysinit" || true
runroot "rc-update add killprocs shutdown; rc-update add mount-ro shutdown"

echo "=== [60] inittab: serial gadget getty ==="
grep -q ttyGS0 "$ROOT"/etc/inittab 2>/dev/null || \
  echo 'ttyGS0::respawn:/sbin/getty -L 115200 ttyGS0 vt100' >> "$ROOT"/etc/inittab

echo "=== [60] fstab / hostname / network ==="
echo t4linux > "$ROOT"/etc/hostname
cat > "$ROOT"/etc/fstab <<'EOF'
/dev/mmcblk0p15  /      ext4  rw,noatime  0 1
tmpfs            /tmp   tmpfs defaults,nosuid,nodev 0 0
EOF
cat > "$ROOT"/etc/network/interfaces <<'EOF'
auto lo
iface lo inet loopback
auto usb0
iface usb0 inet static
    address 10.42.0.1
    netmask 255.255.255.0
EOF

echo "=== [60] Xorg config ==="
mkdir -p "$ROOT"/etc/X11/xorg.conf.d
# PROVEN-WORKING config (validated on-device; do not revert to explicit
# InputDevice sections):
# - AutoAddDevices ON + InputClass: explicit InputDevice entries make evdev
#   open /dev/input/event0 twice (it carries BOTH touch axes and nav keys),
#   which Xorg rejects as "device file is duplicate" -> PreInit 8 -> no pointer.
# - udev must tag event0 as ID_INPUT_TOUCHSCREEN first; see udev-input.start
#   in /etc/local.d (baked below). evdev then reports "Configuring as touchscreen".
# - MatchIsTouchscreen catches only the touch node; MatchIsKeyboard routes the
#   power key (event1) + gpio-keys (event6) to evdev keyboards.
# - no DefaultDepth: 16bpp path SIGILLs in shadowfb; keep fb native 32bpp.
cat > "$ROOT"/etc/X11/xorg.conf.d/10-t4.conf <<'EOF'
Section "ServerFlags"
    Option "AutoAddDevices" "true"
    Option "AutoEnableDevices" "true"
EndSection
Section "Device"
    Identifier "fb"
    Driver "fbdev"
    Option "fbdev" "/dev/fb0"
EndSection
Section "Screen"
    Identifier "s0"
    Device "fb"
EndSection
Section "InputClass"
    Identifier "bt532 touch"
    MatchIsTouchscreen "on"
    MatchDevicePath "/dev/input/event*"
    Driver "evdev"
    Option "EmulateThirdButton" "1"
EndSection
Section "InputClass"
    Identifier "nav keys"
    MatchIsKeyboard "on"
    MatchDevicePath "/dev/input/event*"
    Driver "evdev"
EndSection
Section "ServerLayout"
    Identifier "l0"
    Screen "s0"
EndSection
EOF

echo "=== [60] udev input re-tag (touchscreen classification) ==="
# eudev on this board does not tag input nodes at boot. Without the
# ID_INPUT_TOUCHSCREEN tag, Xorg can't match the InputClass above and the
# touch ends up keyboard-only. Trigger a re-tag at every boot.
mkdir -p "$ROOT"/etc/local.d
cat > "$ROOT"/etc/local.d/udev-input.start <<'EOF'
#!/bin/sh
# re-tag input devices so libinput/evdev classify the touchscreen
udevadm trigger --subsystem-match=input --action=change 2>/dev/null
exit 0
EOF
chmod +x "$ROOT"/etc/local.d/udev-input.start

echo "=== [60] firstboot resize + usb gadget net ==="
mkdir -p "$ROOT"/etc/local.d
cat > "$ROOT"/etc/local.d/t4-firstboot.start <<'EOF'
#!/bin/sh
# one-time: grow rootfs to fill SYSTEM partition
if [ ! -f /etc/t4-resized ]; then
  resize2fs /dev/mmcblk0p15 && touch /etc/t4-resized
fi
# usb0 (g_ncm loaded via /etc/modules): static ip + dhcp server for the host
if [ -d /sys/class/net/usb0 ]; then
  ifconfig usb0 10.42.0.1 netmask 255.255.255.0 up 2>/dev/null
  echo -e 'interface usb0\nstart 10.42.0.100\nend 10.42.0.120\nmax_leases 4\noption subnet 255.255.255.0\noption lease 86400' > /etc/udhcpd.conf
  udhcpd /etc/udhcpd.conf 2>/dev/null
fi
EOF
chmod +x "$ROOT"/etc/local.d/t4-firstboot.start
cat > "$ROOT"/etc/local.d/sd8887.start <<'EOF'
#!/bin/sh
# power on Marvell sd8887 radio + set sta mode (from pmos)
[ -e /sys/devices/platform/sd8x-rfkill/pwr_ctrl ] && echo 1 > /sys/devices/platform/sd8x-rfkill/pwr_ctrl
[ -e /proc/mwlan/config ] && echo drv_mode=1 > /proc/mwlan/config
EOF
chmod +x "$ROOT"/etc/local.d/sd8887.start

echo "=== [60] kernel modules + firmware ==="
KREL=$(make -s -C /work/repo/kernel-degas O=out ARCH=arm kernelrelease 2>/dev/null || echo 3.10.0-t4)
echo "kernel release: $KREL"
mkdir -p "$ROOT"/lib/modules/"$KREL"
cp -a /work/dist/mod/lib/modules/* "$ROOT"/lib/modules/"$KREL"/ 2>/dev/null || true
depmod -b "$ROOT" "$KREL" 2>/dev/null || true
mkdir -p "$ROOT"/lib/firmware
tar -xzf /work/backup/system-mrvl-fw.tgz -C "$ROOT"/lib/firmware --strip-components=2 2>/dev/null || \
  tar -xzf /work/backup/system-mrvl-fw.tgz -C "$ROOT"/lib/firmware
ls "$ROOT"/lib/firmware/ "$ROOT"/lib/modules/ 2>/dev/null
# modprobe configs from pmos
mkdir -p "$ROOT"/etc/modprobe.d
cp /work/build/pmos-files/device/archived/device-samsung-degaswifi/*.conf "$ROOT"/etc/modprobe.d/ 2>/dev/null || true
cat > "$ROOT"/etc/modules <<'EOF'
mlan
sd8887
galcore
EOF

echo "=== [60] cleanup ==="
runroot "apk cache clean" || true
rm -f "$ROOT"/usr/bin/qemu-arm-static "$ROOT"/etc/resolv.conf
rm -rf "$ROOT"/var/cache/apk/*

echo "=== [60] pack ext4 image ==="
bash /work/build/scripts/65-pack-rootfs.sh
echo "=== [60] DONE ==="
