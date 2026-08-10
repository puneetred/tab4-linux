# 12 — GPU acceleration: Vivante GC1000 via stock ROM blobs

**Status: stages 0–4 complete (2026-08-10).** The kernel side works
(galcore 4.6.9.8290 loaded, `/dev/galcore` + `/dev/ion` present, contract
probe verified on device). Clean stock blobs (`T231XXU0ANE2`) are extracted
and version-coupling is proven. The bionic runtime compatibility layer
(stage 2) is working on Alpine, and the stock **libGAL.so runs natively and
round-trips real ioctls to the kernel** (stage 3). **EGL 1.4 + GLES2 from
the stock blobs now run on Alpine and render to the LCD**: offscreen
triangles/pbuffers verified via readback, and a fullscreen 800×1280 GPU
render is blitted to `/dev/fb0` (stage 4).

This document is the master plan. Work proceeds piece by piece, each stage
verifiable on the device.

---

## 1. Verified current state (2026-08-09)

| Component | State | Evidence |
|---|---|---|
| Kernel galcore module | **Working** | `lsmod`: galcore loaded; `/dev/galcore` exists |
| GPU silicon | GC1000 (2× core, `clk1x` @ 416 MHz) | dmesg galcore banner |
| Kernel galcore version | **4.6.9.8290** | dmesg; `gc_hal_version.h` in kernel source |
| Chip config string | `rls_pxa988_KK44_GC13.18` | dmesg |
| Buffer allocator | **ION** | `/dev/ion` exists, `gralloc.mrvl.so` uses ion/pmem |
| ROM blobs (local) | **Corrupt** | `backup/mmcblk0p15-system.img` data region damaged; `/lib` files are zeros/garbage |
| Stock firmware | **Acquired (authentic)** | FUS (samloader-rs) → `T231XXU0ANE2` (exact build in device build.prop); zip/tar verified |
| Blobs extracted | **Clean, verified** | `rom/blobs-clean/`: all ELF magic, ARM EABI5, bionic-built |
| Version coupling | **PROVEN** | blob embeds `$VERSION$4.6.9:8290$` == kernel 4.6.9.8290 |
| Bionic compat layer | **Working** | `/stage2` tree, `/system → /stage2`, `/dev/__properties__` fixed; mksh + toolbox native on Alpine |
| **libGAL bring-up (stage 3)** | **Working** | stock `libGAL.so` dlopen'd from Alpine; `gcoOS_ModuleConstructor`/`gcoHAL_Construct`/queries OK; real ioctls to kernel |
| **EGL/GLES2 (stage 4)** | **Working** | `libEGL_MRVL` + `libGLESv2_MRVL` (GC13.20) run on Alpine: display init, configs, contexts, pbuffers, shaders, draws, readback all OK; fullscreen render blitted to `/dev/fb0` |

## 2. The end-state stack (target)

```
Xorg (fbdev) ── GLX?? ──┬── software (today)
                        └── Vivante GLX (later: Freescale BSP libGL or EGL-based)

EGL/GLESv2 (blobs: libEGL_MRVL.so, libGLESv2_MRVL.so)
   │   libGLESv2SC.so (shader compiler), libGLESv1_CM_MRVL.so
   ▼
libGAL.so (Vivante driver core, blob)
   │   ioctl(fd, 30000/30001, &gcsHAL_INTERFACE)
   ▼
/dev/galcore  ← kernel driver (GPL, in-tree, already built & loaded)
   ▲
ion (gralloc.mrvl.so) /dev/ion ── contiguous GPU memory
```

Everything from `libGAL.so` up is a closed binary from the stock ROM,
compiled against **bionic** (Android libc). Alpine is **musl** — so the
central engineering problem is not the GPU, it is *running bionic-built
binaries on a musl system* (stage 3).

## 3. The kernel ↔ blob contract (reverse-engineered, from kernel source)

The GPL kernel source contains the complete userspace ABI in
`drivers/marvell/graphics/galcore_4x/hal/inc/gc_hal_driver.h`:

* Device node: `/dev/galcore`, class `galcore`.
* Ioctl codes: `IOCTL_GCHAL_INTERFACE = 30000`, `IOCTL_GCHAL_KERNEL_INTERFACE = 30001`.
* Payload: one `gcsHAL_INTERFACE` struct: `{command, hardwareType, status,
  handle, pid, union{...}}`. The union holds per-command structs.
* Command enum `gceHAL_COMMAND_CODES` (order-sensitive ABI):
  `QUERY_VIDEO_MEMORY`, `QUERY_CHIP_IDENTITY`, allocate/free non-paged +
  contiguous + video memory, map/unmap, lock/unlock, `EVENT_COMMIT`,
  `USER_SIGNAL`, `SIGNAL`, `WRITE_DATA`, `COMMIT`, `STALL`,
  `READ/WRITE_REGISTER`, power management, `QUERY_KERNEL_SETTINGS`, `RESET`,
  `MAP_PHYSICAL`, `DEBUG`, `CACHE`, `TIMESTAMP`, `DATABASE`, **`VERSION`**,
  `CHIP_INFO`, `ATTACH`, `DETACH`, `COMPOSE`, ... `CREATE_NATIVE_FENCE`.
* Version handshake: the blob queries `gcvHAL_VERSION` and compares against
  its own build; a userspace/kernel mismatch is refused. **The kernel we ship
  is 4.6.9.8290 — the blobs must expect the same (or a compatible) version.**

Kernel-side entry point: `drv_ioctl()` in `gc_hal_kernel_driver.c`.

**Verified in the blob binary too** (2026-08-09): `libGAL.so` contains
literal-pool constants `0x7530`/`0x7531` (= 30000/30001) inside `gcoHAL_Call`,
its userspace ioctl dispatcher, and the strings `/dev/galcore`,
`/dev/graphics/galcore`, `HAL kernel version %d.%d.%d build %u`,
`gcvSTATUS_VERSION_MISMATCH`. Contract matches the kernel source exactly.

## 4. The blob inventory (stock T231XXU0ANE2 / AOI2 4.4.2)

From `proprietary-blobs.txt` and the corrupted dump's inode list:

| Blob | Role |
|---|---|
| `libGAL.so` | Driver core: ioctls, chip init, command queues |
| `libEGL_MRVL.so` | EGL 1.4 implementation |
| `libGLESv1_CM_MRVL.so` / `libGLESv2_MRVL.so` | GLES 1.1 / 2.0 |
| `libGLESv2SC.so` | GLSL→native shader compiler |
| `libGLES_android.so` | swrast fallback (GLES 1.0) |
| `libHWComposerGC.so` + `hw/hwcomposer.mrvl.so` | display composition |
| `hw/gralloc.mrvl.so` (+ `gralloc.default.so`) | buffer allocation via ion |
| `libion.so`, `libionhelper.so` | ION userspace |

Plus bionic runtime libs for the compatibility layer (stage 3).

### Dependency closure (verified with readelf, `rom/bionic/`)

```
libGLESv2_MRVL → libGLESv1_CM_MRVL → libEGL_MRVL → libGAL → libgputex
libGAL → libcutils, libutils, libdl, liblog, libgputex, libc, libstdc++, libm
libEGL_MRVL → (+ libhardware)
gralloc.mrvl → libGAL, libgcu, libbinder, libmvmem, libutils, libcutils, liblog
libgcu → libGAL
libmvmem → libion (→ /dev/ion — present in our kernel!)
```

Full bionic runtime set extracted: `libc, libdl, libm, libstdc++, liblog,
libcutils, libutils, libhardware, libbinder, libgputex, libgcu, libmvmem,
libion` + `/system/bin/linker` (all from the same ANE2 build, self-consistent).

**Version coupling proven**: `strings libGAL.so | grep VERSION` →
`$VERSION$4.6.9:8290$` — the blob expects exactly the galcore version the
kernel ships. No kernel patch needed; risk #1 retired.

## 5. Stage plan (each stage verifiable on device)

### Stage 0 — Foundation (this document)
* [x] Kernel galcore verified loaded; version pinned to 4.6.9.8290
* [x] ROM blob inventory from corrupted dump (names/sizes only)
* [x] Kernel↔blob ABI extracted from GPL source (`gc_hal_driver.h`)
* [x] Clean blobs from stock firmware (FUS `T231XXU0ANE2`, zip+tar verified)
* [x] ARM RE toolchain container (t4build: binutils-armhf readelf/objdump/strings)
* [x] Static analysis: blob version strings, dependencies, ioctl usage
* [x] **Verify version coupling: blob `$VERSION$4.6.9:8290$` == kernel 4.6.9.8290**

### Stage 1 — Kernel contract probe (no blobs needed)
**DONE (2026-08-09)** — `tools/gpu/probe/src/galcore-probe.c`, static armhf,
ran on the device (`ssh root@10.42.0.1`):
1. `gcvHAL_QUERY_CHIP_IDENTITY` → chipModel 0x1000 (GC1000), revision 0x5037,
   features 0xe0286cad, minor 0xe1799eff, minor1 0xbe13b2d9, 2 shader cores,
   512 threads, streamCount 4, registerMax 64, vertexCacheSize 8, pixelPipes 1,
   instructionCount 256, numConstants 576, varyingsCount 8
2. `gcvHAL_VERSION` → kernel galcore **4.6.9 build 8290**
3. `gcvHAL_QUERY_VIDEO_MEMORY` → internal 0/0, external 0/0, contiguous
   phys=0xffffffffe913d100 size=67108864 (64 MB)
Rebuild: `tools/gpu/probe/build.sh` (docker t4build). Ioctl payload is the
`DRIVER_ARGS{InBuf,InSize,OutBuf,OutSize}` wrapper around `gcsHAL_INTERFACE`
(found by reading `gc_hal_kernel_os.h`; raw interface passes gave `-ENOTTY`).

### Stage 2 — Runtime compatibility layer (the hard part)
**DONE (2026-08-09)** — no chroot needed: the bionic ELFs hardcode
`/system/bin/linker`, so a symlink suffices:
* Deployed set `rom/stage2/` (30 files, 5.7 MB, `stage2.tar.gz` md5
  `3c0ac7b6145bb6ed8d46fe7086dbf371`) → device `/stage2`, `ln -sfn /stage2 /system`.
* Components: `bin/{linker, mksh, sh→mksh, toolbox, app_process}` + `lib/`
  (bionic libc/libdl/libm/libstdc++/liblog/libcutils/libutils/libhardware/
  libbinder, plus blob deps libGAL, libgputex, libgcu, libmvmem, libion,
  libusbhost, libselinux, libpcre, libcrypto) + `lib/egl/`, `lib/hw/`.
* **Samsung property-area quirk**: `__system_properties_init` (libc+0x2ee00)
  requires `/dev/__properties__` to exist with **magic `PROP` at offset +8**
  (not +0) and version `0xfc6ed0ab` at +12 — Samsung shifted the header.
  Without it every bionic binary SIGSEGVs at `ldr r3,[r6,#16]` (fault 0x90).
* Smoke tests pass natively on Alpine 3.20: `/system/bin/mksh`, `toolbox id`
  (uid=0, full groups, context=kernel), `toolbox getprop`.
* strace for the device (Alpine pkg chain): strace-6.9-r0, libdw-0.191-r0,
  libelf-0.191-r0, musl-fts-1.2.7-r6, bzip2-1.0.8-r6, xz-libs-5.8.3-r0.
* Original plan (below) kept for history: option A was adopted, without the
  chroot (symlink trick replaces it).
The blobs need bionic. Two candidate architectures:
* **A. Bionic chroot**: mount stock `/system` (from firmware `system.img`)
  read-only; run test ELF with the ROM's `/system/bin/linker` + `libc.so`.
  Zero porting; pure reverse engineering. Needs a property shim or stubs.
* **B. libhybris on a glibc side-rootfs**: well-trodden (Ubuntu Touch era);
  needs a glibc chroot (Debian armhf) plus hybris build. Heavier.
Decision gate: try A first (closest to "reverse engineer the blobs").
The galcore 4.6.9.8290 version gate is **already known to match** (stage 0) —
no kernel patching expected.

### Stage 3 — libGAL bring-up
Minimal Android executable (or hybris app): dlopen `libGAL.so`, call
`gcoOS_*`/`gckHARDWARE_*` entry points, verify chip query + version handshake
against `/dev/galcore`. Milestone: first kernel↔blob round trip.

**Status: COMPLETE (2026-08-09)** — see `tools/gpu/galtest/`. Verified on
device with the stock blob running on the bionic compat layer:

* `gcoOS_ModuleConstructor(pid)` (not `gcoOS_Construct` alone!) is the
  correct entry — it creates the pthread key that `gcoOS_GetTLS` needs, then
  constructs the OS. Direct `gcoOS_Construct` left `pthread_getspecific`
  with no key → all queries failed with `-7 NOT_SUPPORTED`.
* Samsung's ABI differs from the GPL headers — **all `gcoHAL_*` query
  wrappers ignore r0 and forward extra args**: 
  `gcoHAL_QueryChipIdentity(0, out, 0, 0, 0)` (writes up to 8 out pointers),
  `gcoHAL_QueryVideoMemory(0, &iPhys, &iSize, &ePhys, &eSize, &cPhys, &cSize)`
  (7 args), `gcoHAL_Construct(0, 0, &hal)` (out pointer in r2).
  Calling with the GPL 2-arg pattern shifts r0→r1 and crashes on a
  libGAL-text write (verified via core dumps + NT_PRSTATUS).
* Kernel ground truth (raw GPL ioctl, same process): `QUERY_CHIP_IDENTITY`
  reply has the identity **at reply+0x24** (0x1000=GC1000, rev 0x5037,
  features 0xE0286CAD, minor 0xE1799EFF); `QUERY_VIDEO_MEMORY`: internal=0,
  external=0, contiguous phys=0xE913D100 size=64 MB.
* The blob's own `gcoHAL_QueryChipIdentity` reports `chipModel=0x320` —
  Samsung's closed user struct reads a reply padding field (kernel stack
  noise), not the GPL field; treat blob-reported identity as unreliable,
  kernel/probe identity as truth. Video memory via the blob **matches the
  raw probe exactly** (64 MB contiguous).
* Full round trip: 17+ ioctls (all `0x7530`, all processed, strace-verified),
  `gcoHAL_Construct`/`Destroy` OK, `gcoOS_Destroy` NOT called explicitly —
  the blob registers an atexit destructor; a manual destroy caused a double-
  destroy SIGSEGV at process exit (fd fetch `ldr r0,[r3,#20]`, r3=NULL).
* Test is bionic-linked (`_start` → `main` → `exit`, raw `write()` for
  output; no stdio). Exit code 0, clean exit.

### Stage 4 — EGL on the framebuffer
**Status: COMPLETE (2026-08-10)** — `tools/gpu/egltest/`. The stock
`libEGL_MRVL.so` + `libGLESv2_MRVL.so` now run on Alpine, and the GPU
renders to the LCD. What it took:

* **Missing dep**: `eglCreateContext` dlopens the system wrapper
  `libGLESv2.so` → `libEGL.so` → `libGLES_trace.so` → **`libstlport.so`**
  (Samsung's STL). Extracted from `system.raw.img` (debugfs) and deployed;
  before that context creation failed with `gcvSTATUS_NOT_FOUND` (-19).
* **Blob logging**: no logd socket on the device, so `__android_log_print`
  went nowhere. `logshim.so` (LD_PRELOAD shim exporting the liblog entry
  points, printing to stdout) made every internal Vivante trace visible —
  this is what surfaced the libstlport chain and the "OES20 ===> GC Version
  rls_pxa988_KK44_GC13.20" driver banner.
* **Linker gotchas** (Samsung bionic 4.4 linker):
  * `RTLD_NOW|RTLD_GLOBAL` → "invalid flags to dlopen: 102" — use plain
    `RTLD_NOW` (galtest pattern).
  * Shared libs need a **SysV DT_HASH**: without `-Wl,--hash-style=sysv` the
    loader rejects with "empty/missing DT_HASH".
  * EGL blobs live in `/system/lib/egl/` — dlopen the full path.
* **ABI notes**: `eglInitialize`/`eglMakeCurrent`/`eglSwapBuffers` return
  `EGLBoolean` (1), NOT `EGLint` error codes — treat return as bool, check
  `eglGetError` separately. `eglChooseConfig` returns config **indices**
  (small ints), not pointers.
* **Verified pipeline** (all on device, readback-verified):
  1. EGL 1.4, VENDOR "Vivante Corporation", full extension list.
  2. Context + pbuffer + makeCurrent OK; shaders compile/link (GC13.20).
  3. Fullscreen triangle → 64/64 green pixels; 800×1280 gradient render →
     readback OK.
  4. **fb0 blit**: render 800×1280, `glReadPixels` → RGBA→BGRA byte swap
     (panel format `rgba 8/16,8/8,8/0,8/24` = BGRA8888), memcpy to
     `/dev/fb0` mmap — 4,096,000 bytes verified on re-read. The LCD shows
     the GPU-rendered gradient (backlight `brightness`=143/255).
* **KNOWN QUIRK — `glClear` writes nothing**: `glClear(GL_COLOR_BUFFER_BIT)`
  produces all-zero readback (before AND after successful draws) while
  triangle draws on the same surface work perfectly. No GL errors, no kernel
  faults, ioctls all succeed. The clear path in this Samsung stack silently
  misses the surface target. Workaround: draw a fullscreen quad instead of
  clearing. Under investigation; harmless for the current milestone.
* Backlight: `/sys/class/backlight/panel/{brightness,max_brightness}`.

Original plan (below) kept for history — the native-window shim was
*not* needed for the milestone: rendering into an 800×1280 pbuffer +
readback + fb0 blit bypasses gralloc/ANativeWindow entirely.
Two paths:
* Vivante's `libEGL` platform selection reads `/system/lib/egl/egl.cfg` and
  the platform via `gcoHAL`/`fbdev` entry points in these blobs — reverse
  engineer which platform backend these blobs support (likely only Android
  ANativeWindow → needs a minimal `gralloc` + buffer-present path to fbdev).
* Or write a native-window shim: EGL renders into ion/gralloc buffers;
  a small host-side presenter blits the rendered buffer to `/dev/fb0`.
(Still relevant later for real windowed compositing: `eglCreateWindowSurface`
→ `veglGetWindowInfo` → ANativeWindow/gralloc; swap path uses a worker
thread + `veglSynchronousFlip`/display HAL.)

### Stage 5 — Desktop integration
* Xorg GLX: Vivante GLX module (Freescale BSP `libGL` + X driver) is glibc —
  favours architecture B; or
* Wayland-less EGL compositing (e.g. `weston` on fbdev with EGL backend is
  not blob-friendly) — likely **Xorg + GLX through the Vivante stack** or a
  headless GPU render server (GLES apps on the tablet, buffers presented via
  a small fbdev blitter).

### Stage 6 — Polish
* ~~Version-verify against stock ANE2 blobs~~ DONE (exact build match).
* Optional: mainline `etnaviv` DRM backport to 3.10 as a long-term
  *open-source* replacement (GC1000 is in etnaviv's hwdb); large effort.

## 6. Files & artifacts

| Path | Contents |
|---|---|
| `~/Documents/tab4/rom/blobs-clean/` | clean blobs (libGAL.so, egl/, hw/) from FUS system.img |
| `~/Documents/tab4/rom/bionic/` | bionic runtime set (libc…linker) for stage 3 |
| `~/Documents/tab4/rom/blobs/` `egl/` `hw/` | corrupt copies from backup dump (keep as diff source) |
| `~/Documents/tab4/fw/` | FUS zip, AP tar.md5, `system.img`, `system.raw.img`, `check_sparse.py` |
| `tools/gpu/probe/` | Stage-1 probe: `src/galcore-probe.c`, `inc/` (kernel headers), `build.sh` |
| `tools/gpu/galtest/` | Stage-3 test: `galcore-test.c`, `crte.S` (minimal `_start`), `build.sh` |
| `tools/gpu/egltest/` | Stage-4 test: `egl-test.c` (EGL+GLES2 render + fb0 blit), `logshim.c` (LD_PRELOAD log shim), `crte.S`, `build.sh` |
| `docs/12-gpu.md` | this document |

Firmware provenance: `fw/SM-T231_1_20150911094727_d0bnw4d7b4_fac.zip`
(zip -t OK) → `AP_T231XXU0ANE2_1494982_REV03_user_low_ship.tar.md5`
(md5 `a417f5d3101795cc4d4ca425c6359188`) → `system.img` (1732808544 B,
Android sparse, 506112×4096 blk, 2561 chunks) → simg2img → debugfs dump.
Note: the earlier samsony.net `KSA-T231XXU0AOI2` download was corrupt (bad
embedded md5) — deleted.

## 7. Risks

1. ~~**Version gate**~~ **RETIRED (2026-08-09)**: blob `$VERSION$4.6.9:8290$`
   == kernel 4.6.9.8290 — same Vivante release.
2. **bionic runtime friction**: property service, logd, missing HAL
   interfaces → stub them (blobs degrade gracefully; GAL core needs very few).
   Mostly solved in stage 2/3: property area quirk fixed; logd opens fail
   gracefully (`/dev/log/main` ENOTDIR — harmless).
3. **No ANativeWindow**: blobs may hard-require gralloc/buffers for EGL
   surfaces → stage 4 shim.
4. ~~**Newer AOI2 blobs vs NE2-era kernel**~~ **RETIRED**: blobs are the exact
   `T231XXU0ANE2` build this unit shipped with (FUS, verified).
5. **Closed-blob ABI drift (NEW)**: Samsung's user-space `gcsHAL_INTERFACE`
   reply layout differs from the GPL kernel header (blob reads chip identity
   from reply padding → `0x320` vs kernel's `0x1000`). Kernel truth is
   verified independently (raw probe + dmesg); any blob mis-reads must be
   worked around in stage 4 (EGL layer), not trusted.
6. **Version gate on EGL layer**: same `$VERSION$` string must also match in
   `libEGL_MRVL`/`libGLESv2_MRVL` — spot-check before stage 4 (they are the
   same firmware build, so expected OK).
7. **`glClear` broken in the Samsung GLES2 blob (stage 4)**: color clears
   write nothing (readback all-zero, no errors/faults); draws work. Any
   consumer must draw fullscreen geometry instead of relying on clears.
8. **EGL blobs pull the whole Android wrapper chain**: `libGLESv2_MRVL`
   expects the system `libGLESv2.so`/`libEGL.so` wrappers (and
   `libGLES_trace.so` → `libstlport.so`) to exist; the wrappers' dlopen
   dispatch (egl.cfg) is what wires vendor libs together — all shipped in
   stage 2, but any future blob update must re-check the closure.
