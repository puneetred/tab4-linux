# References & attribution

This port stands on prior work. Key sources:

* **Kernel**: [`Sabsabsab/android_kernel_samsung_degas3g`](https://github.com/Sabsabsab/android_kernel_samsung_degas3g)
  (Samsung degas3g / PXA1088 kernel, 3.10).
* **postmarketOS pmaports**: `device/archived/device-samsung-degaswifi/` and
  `device/archived/linux-samsung-degaswifi/` —
  * kernel gcc/backport patch set,
  * **UCM2 audio config** (`audio/ucm2/pxa-88pm805-dkb-hifi/HiFi.conf`) — the
    authoritative 88pm805 register bring-up used in `t4sound.start`,
  * WiFi (sd8887) firmware list + modprobe confs + rfkill/drv_mode sequence.
* **Boot image tooling**: [`osm0sis/pxa-mkbootimg`](https://github.com/osm0sis/pxa-mkbootimg)
  (PXA1088 DTB-aware mkbootimg + `pxa1088-dtbTool`).
* **Rootfs**: [Alpine Linux](https://alpinelinux.org) 3.20.3 armhf minirootfs + XFCE4.
* **busybox** 1.36.1 (static arm) for the initramfs.

Everything in `build/`, `tools/`, `patches/`, `rootfs-overlay/`, and `docs/` was
authored for this port. Reverse-engineered findings (Xorg/musl, DefaultDepth16,
compositor, codec readback quirk, evdev/libinput touch) are documented in `docs/`.

## License note

Kernel patches derive from GPL kernel sources (GPL-2.0). The custom scripts,
docs, and config here are provided as-is for educational/repair use on hardware
you own. Respect the licenses of the upstream projects above.
