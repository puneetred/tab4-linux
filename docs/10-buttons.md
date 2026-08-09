# 10 — Buttons (power / volume / home / back / recent)

## Where each button lives (keycode → keysym)

| Button | Input device | Linux keycode | X keycode | X keysym |
|---|---|---|---|---|
| **Power** | `88pm80x_on` (event1) | KEY_POWER (116) | 124 | XF86PowerOff |
| **Volume Down** | `gpio-keys` (event6) | KEY_VOLUMEDOWN (114) | 122 | XF86AudioLowerVolume |
| **Volume Up** | `gpio-keys` (event6) | KEY_VOLUMEUP (115) | 123 | XF86AudioRaiseVolume |
| **Home** | `gpio-keys` (event6) | KEY_HOME (102) | 110 | Home |
| **Back** | `sec_touchscreen` (event0) | KEY_BACK (158) | 166 | XF86Back |
| **Recent** | `sec_touchscreen` (event0) | BTN_4 (254) | — | (mouse button 8) |

Decode the live capabilities (note: busybox prints the bitmask **high word first**):

```sh
cat /sys/class/input/event0/device/capabilities/key
```

## Working bindings (xfce4-keyboard-shortcuts → /commands/custom)

* `XF86AudioRaiseVolume` → `/usr/local/bin/t4vol up`
* `XF86AudioLowerVolume` → `/usr/local/bin/t4vol down`
* `XF86PowerOff` → `xfce4-session-logout`  (session dialog; safe/recoverable)

These are grabbed by **xfsettingsd** and persist in
`root/.config/xfce4/xfconf/xfce-perchannel-xml/xfce4-keyboard-shortcuts.xml`.

## The power-button gotcha (important)

**`xfce4-power-manager` grabs the power key and calls logind/systemd — which
doesn't exist here — then swallows the key silently.** Symptom: power button
"does nothing" even though keycode 124 clearly reaches X (`xinput test-xi2`).

Fix: kill it and stop it autostarting:
```sh
pkill xfce4-power-manager     # note: name >15 chars, use pkill -f or the PID
printf '[Desktop Entry]\nHidden=true\n' > ~/.config/autostart/xfce4-power-manager.desktop
```
Then xfsettingsd's `XF86PowerOff` binding fires.

## Still to do

* **Home** currently emits `Home` to the focused app (can feel like a click);
  bind it to something tablet-sensible (`wmctrl -k on` = show desktop).
* **Back** → e.g. `xdotool key Escape` or `alt+Left`.
* **Recent** (BTN_4, a *mouse* button) needs `xbindkeys` to map to a window
  switcher (`xfce4-popup-windowmenu` / wmctrl).
