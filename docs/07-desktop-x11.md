# 07 — Desktop / X11: three defects with identical symptoms

Three independent root causes prevented Xorg from staying up on this stack. All
three presented with the same symptom (`Illegal instruction at 0xb6??674` or a
black screen), which made isolation non-trivial.

---

## Bug 1 — Xorg SIGILL at `...674` (musl `a_crash`, not pixman)

**Symptom:** Xorg aborts right after `Initializing extension SHAPE` /
`DRI2`/`AIGLX` with `Illegal instruction at address 0xb6XX674`. The address
*moves* run to run (ASLR) but always ends in `0x674`.

**Initial hypothesis (incorrect):** the fault addr mapped into libpixman, and
disassembly showed NEON combiners → looked like a classic "NEON probing on a
broken NEON" SIGILL. Rebuilt libpixman with `-Dneon=disabled -Darm-simd=disabled`
→ **still crashed**. So pixman wasn't the culprit.

**Real root cause (how it was proven):**
1. Captured a core: `ulimit -c unlimited; echo /tmp/core > /proc/sys/kernel/core_pattern; Xorg ...`.
2. Parsed the ELF core's **NT_FILE note** (python, no gdb needed) to map the
   fault vaddr → **file + offset**: it landed in `/lib/ld-musl-armhf.so.1`, not pixman.
3. `objdump` at that offset → `e7f000f0  udf #0` → that's musl's **`a_crash()`**,
   the deliberate trap musl's mallocng hits on **heap corruption / bad state**.

**Conclusion:** the SIGILL was musl's allocator tripping, triggered by something
in Xorg's **udev input-probing** path (dlopen of input drivers against a kernel
without a working udev at the time). It was *not* a CPU/NEON issue at all.

**Fix:** two parts —
* Install **eudev** and let input devices be **tagged by udev** (`ID_INPUT_*`),
  then use **libinput**'s catchall (`/usr/share/X11/xorg.conf.d/40-libinput.conf`).
* Keep `AutoAddDevices` default (on) once udev works; the musl crash was in the
  *manual/probe* path, which udev+libinput avoids.

(The pure-C libpixman is still installed — harmless, and removes the red herring.)

---

## Bug 2 — `DefaultDepth 16` SIGILL (16bpp shadowfb)

**Symptom:** after fixing #1, Xorg with my hand-written `10-fbdev.conf` crashed
again at `...674` — but a *minimal* config worked.

**Isolation:** bisected the config. Adding `DefaultDepth 16` (with a `Modes`
subsection) reliably re-triggered the SIGILL.

**Root cause:** the 16bpp path in fbdev/shadowfb on this stack is broken. The
panel is natively 32bpp; forcing 16bpp walks a buggy conversion path.

**Fix:** **don't set `DefaultDepth`/`Modes`** — let fbdev negotiate the native
32bpp mode. The shipped `10-t4.conf` has Device+Screen only, no depth override.

---

## Bug 3 — xfwm4 black screen (compositor wants GL)

**Symptom:** Xorg alive, `xfce4-session`/`xfwm4`/`xfdesktop`/`xfce4-panel` all
running, cursor visible — but **nothing paints** (black screen).

**Root cause:** xfwm4's **compositor** tries to use GLX → AIGLX → `swrast` —
which doesn't exist on this musl/fbdev stack. The compositor fails and leaves
the screen uncomposited/black.

**Fix:** disable compositing:
```sh
xfconf-query -c xfwm4 -p /general/use_compositing -s false
```
(baked into `rootfs-overlay/root/.config/xfce4/.../xfwm4.xml`). Immediately the
wallpaper, panel, and desktop icons render.

---

## Display manager: bypassed

lightdm works up to the greeter, but **autologin** needs AccountsService/logind
(absent without systemd), and there's no keyboard to type a password. Simplest
deterministic path: an OpenRC service `t4desktop` that runs
`startx :0 vt07` with `/root/.xinitrc = startxfce4`. No DM, no PAM, no greeter.

## Result

Full XFCE4: Applications menu, panel, desktop icons, window manager, touch.
Screenshot proof captured via `fbgrab` and pulled over `nc`.
