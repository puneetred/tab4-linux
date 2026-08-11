# 08 — Touchscreen (sec_touchscreen / Zinitix bt532)

## The device

`/dev/input/event0`, name `sec_touchscreen`. Besides touch axes it also carries
the **capacitive Back (KEY_BACK=158)** and **Recents (BTN_4=254)** keys — they
show up as a second (keyboard) X input device.

Absolute ranges (from `evtest`): `ABS_X 0..798`, `ABS_Y 0..1278` ≈ the 800×1280
panel (no calibration needed).

## Two ways to drive it

### A) libinput (what we use)

Once **eudev** is running, the device gets tagged `ID_INPUT_TOUCHSCREEN=1` and
Xorg's libinput catchall (`40-libinput.conf`) binds it as a proper touchscreen.
Touch→click, pointer-follow, and the nav keys all work through libinput's
virtual subdevices. This is the clean path.

### B) evdev + the kernel classic-axes patch

evdev wants `ABS_X/ABS_Y + BTN_TOUCH`. Stock bt532 only sends `ABS_MT_*`, so we
patched the driver (patches/kernel/bt532-x11-classic-axes.patch) to mirror
finger-0 onto the classic axes. With that, evdev reports
"Found absolute touchscreen / Configuring as touchscreen" and drives the pointer.

## What did NOT work (learn the hard way)

* **evdev on the MT stream without the patch** → touch arrived as XI2
  `RawTouchBegin/End` but **0 ButtonPress** — XFCE never saw a click. This is
  the classic "evdev touchscreen doesn't emulate buttons under XI2" trap.
* **libinput without udev** → `libinput bug: udev device never initialized` /
  `Invalid path` — libinput *requires* a udev-tagged device. Installing eudev
  (and letting it tag the node) is what unlocked libinput.

## Gotcha: do NOT declare the touch as an explicit `InputDevice`

event0 carries **both** touch axes and the Back/Recents keys. If you list it in a
`ServerLayout` as an explicit `InputDevice` (evdev), Xorg opens the node twice
(once as keyboard, once as pointer) and evdev rejects the second open:

```
(WW) evdev: touch: device file is duplicate. Ignoring.
(EE) PreInit returned 8 for "touch"
```

Result: the device registers only as a **keyboard** (id X, type KEYBOARD) and
touch produces **zero pointer motion**. Use **`AutoAddDevices` on + `InputClass`**
instead — udev tags event0 `ID_INPUT_TOUCHSCREEN`, the `bt532 touch` InputClass
binds it as a touchscreen, and `nav keys` routes the key events. See the shipped
`10-t4.conf` and `udev-input.start`.

Note: eudev on this board does **not** tag input nodes at boot by itself — run
`udevadm trigger --subsystem-match=input --action=change` once (baked into
`/etc/local.d/udev-input.start`) so the touchscreen match works after reboot.

## Verify

```sh
xinput list                          # sec_touchscreen as slave pointer + keyboard
xinput test-xi2 --root               # tap → TouchBegin/Motion/ButtonPress
udevadm info /dev/input/event0 | grep ID_INPUT_TOUCHSCREEN
```
