/* t4ioctl.c — LD_PRELOAD shim that logs GAL ioctls to /dev/galcore.
 * Prints gcvHAL_COMMIT command buffers (raw words) to stderr (or a file).
 * Struct layouts verified against galcore 4.6.9 unified_reg kernel source
 * (gc_hal_driver.h gcsHAL_COMMIT + arch/unified_reg/.../gc_hal_user_buffer.h
 * struct _gcoCMDBUF).
 *
 * STDLIB-FREE BY DESIGN: the ANE2-era bionic libc has no relocatable
 * `stderr`/`stdout` globals (bionic exposes `__stderrp`, and "stderr" is
 * only a macro). A glibc-compiled reference to the literal symbol `stderr`
 * fails at LD_PRELOAD time with `cannot locate symbol "stderr" referenced
 * by "libt4ioctl.so"`. So: open()+write() only.
 *
 * Build (bionic, like gpudemo):
 *   arm-linux-gnueabihf-gcc -c t4ioctl.c -o t4ioctl.o -O2 -marm -fPIC \
 *       -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
 *   arm-linux-gnueabihf-gcc -shared -o libt4ioctl.so t4ioctl.o -ldl \
 *       -L<bionic> -lc -Wl,--hash-style=sysv
 * NOTE: --hash-style=sysv is REQUIRED: the 4.4-era bionic linker only
 * reads DT_HASH, not DT_GNU_HASH — a gnu-hash .so fails to LD_PRELOAD
 * ("empty/missing DT_HASH ... built with --hash-style=gnu?").
 * Run: LD_PRELOAD=/tmp/libt4ioctl.so LD_LIBRARY_PATH=/system/lib /tmp/gpudemo
 * Log: stderr by default; set T4IOCTL_LOG=<path> for a file (O_TRUNC),
 *      T4IOCTL_DUMP=0 to suppress the raw-word hexdump (headers only).
 */
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dlfcn.h>

/* gceHAL_COMMAND_CODES (gc_hal_driver.h) */
#define HAL_COMMIT 19

/* gcsOBJECT = magic u32 + type u32 (8 bytes) */
struct gco_cmdbuf {
    uint32_t object_magic;
    uint32_t object_type;
    uint32_t entryPipe;
    uint32_t exitPipe;
    uint32_t using2D;
    uint32_t using3D;
    uint32_t usingFilterBlit;
    uint32_t usingPalette;
    uint32_t physical;      /* gctPHYS_ADDR = void* (4B) */
    uint32_t logical;       /* gctPOINTER */
    uint32_t bytes;         /* gctSIZE_T */
    uint32_t startOffset;
    uint32_t offset;
    uint32_t free;
    uint32_t lastReserve;
    uint32_t lastOffset;
    uint32_t node;
};

/* gcsHAL_INTERFACE: command(4) hardwareType(4) status(4) handle(4) pid(4) union */
struct gal_interface {
    uint32_t command;
    uint32_t hardwareType;
    uint32_t status;
    uint32_t handle;
    uint32_t pid;
    union {
        struct {
            uint32_t context;
            uint32_t commandBuffer;  /* user ptr to gco_cmdbuf */
            uint32_t delta;
            uint32_t queue;
        } commit;
        uint32_t raw[32];
    } u;
};

/* galcore 4.6.9 unified_reg ioctl ABI (verified in blob + kernel source):
 * ioctl(fd, 30000|30001, &DRIVER_ARGS) — NOT &gcsHAL_INTERFACE.
 * The kernel (gc_hal_kernel_os.h) and the blob both use DRIVER_ARGS:
 *   { InputBuffer = &gcsHAL_INTERFACE, InputBufferSize,
 *     OutputBuffer, OutputBufferSize }
 */
struct driver_args {
    uint32_t inputBuffer;       /* +0  gcsHAL_INTERFACE*        */
    uint32_t inputBufferSize;   /* +4                           */
    uint32_t outputBuffer;      /* +8                           */
    uint32_t outputBufferSize;  /* +12                          */
};

static int uptr_ok(uint32_t p)  /* plausible user VA in 1 GB split */
{
    return p >= 0x00010000u && p < 0xBFFFFFFFu;
}

static int (*real_ioctl)(int, unsigned long, ...) = 0;
static int logfd = -1;              /* set on first ioctl */
static int dump_words = 1;          /* set env T4IOCTL_DUMP=0 to suppress hex */
static uint32_t skip_cmd = 0xFFFFFFFFu; /* env T4IOCTL_SKIP=<cmd> to silence a cmd */

/* ---- stdio-free output helpers ---- */

static void wstr(const char* s)
{
    const char* p = s;
    while (*p) p++;
    if (logfd >= 0) write(logfd, s, (size_t)(p - s));
}

static void whex32(uint32_t v)      /* %08x */
{
    char b[9];
    int i;
    for (i = 7; i >= 0; i--) { b[i] = "0123456789abcdef"[v & 0xf]; v >>= 4; }
    b[8] = '\0';
    wstr(b);
}

static void wdec32(uint32_t v)      /* %u */
{
    char b[11];
    int i = 10;
    b[i--] = '\0';
    if (v == 0) b[i--] = '0';
    while (v) { b[i--] = '0' + (v % 10); v /= 10; }
    wstr(b + i + 1);
}

static void wdec64(uint64_t v)      /* %llu (request codes are small) */
{
    char b[24];
    int i = 23;
    b[i--] = '\0';
    if (v == 0) b[i--] = '0';
    while (v) { b[i--] = '0' + (v % 10); v /= 10; }
    wstr(b + i + 1);
}

static uint32_t wparse_dec(const char* s)   /* strtoul-ish */
{
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (uint32_t)(*s++ - '0');
    return v;
}

static void hexdump(const uint32_t* w, int n)
{
    int i, j;
    for (i = 0; i < n; i += 8) {
        wstr("    ");
        whex32((uint32_t)(i * 4));
        wstr(":");
        int m = (n - i < 8) ? n - i : 8;
        for (j = 0; j < m; j++) { wstr(" "); whex32(w[i + j]); }
        wstr("\n");
    }
}

int ioctl(int fd, unsigned long request, ...)
{
    va_list ap;
    void* arg;
    va_start(ap, request);
    arg = va_arg(ap, void*);
    va_end(ap);

    if (!real_ioctl) {
        real_ioctl = (int (*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
        if (logfd < 0) {
            const char* d = getenv("T4IOCTL_DUMP");
            dump_words = (!d || d[0] != '0');
            const char* s = getenv("T4IOCTL_SKIP");
            if (s && *s) skip_cmd = wparse_dec(s);
            const char* p = getenv("T4IOCTL_LOG");
            if (p && p[0]) {
                logfd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            } else {
                logfd = 2;          /* stderr fd */
            }
        }
    }

    if (request == 30000 || request == 30001) {
        struct driver_args* da = (struct driver_args*)arg;
        if (arg && uptr_ok(da->inputBuffer) && da->inputBufferSize >= 24) {
            struct gal_interface* iface = (struct gal_interface*)(uintptr_t)da->inputBuffer;
            if (iface->command != skip_cmd) {
            wstr("== ioctl(fd="); wdec32((uint32_t)fd);
            wstr(", req=");       wdec64((uint64_t)request);
            wstr(") ifsz=");      wdec32(da->inputBufferSize);
            wstr(" cmd=");        wdec32(iface->command);
            wstr(" hw=");         wdec32(iface->hardwareType);
            wstr(" status=");     wdec32(iface->status);
            wstr("\n");
            if (iface->command == HAL_COMMIT && uptr_ok(iface->u.commit.commandBuffer)) {
                struct gco_cmdbuf* cb = (struct gco_cmdbuf*)(uintptr_t)iface->u.commit.commandBuffer;
                wstr("   COMMIT cmdbuf="); whex32((uint32_t)(uintptr_t)cb);
                wstr(" type=");   wdec32(cb->object_type);
                wstr(" entry=");  wdec32(cb->entryPipe);
                wstr(" exit=");   wdec32(cb->exitPipe);
                wstr(" 2d=");     wdec32(cb->using2D);
                wstr(" 3d=");     wdec32(cb->using3D);
                wstr(" fb=");     wdec32(cb->usingFilterBlit);
                wstr(" pal=");    wdec32(cb->usingPalette);
                wstr("\n");
                wstr("   logical="); whex32(cb->logical);
                wstr(" bytes=");  wdec32(cb->bytes);
                wstr(" start=");  wdec32(cb->startOffset);
                wstr(" off=");    wdec32(cb->offset);
                wstr("\n");
                if (dump_words && uptr_ok(cb->logical) && cb->offset <= cb->bytes &&
                    cb->bytes <= (1u << 26)) {
                    uint32_t* words = (uint32_t*)(uintptr_t)cb->logical;
                    int n = (int)((cb->offset ? cb->offset : cb->bytes) / 4);
                    hexdump(words, n);
                }
            }
            }
        } else {
            wstr("== ioctl(fd="); wdec32((uint32_t)fd);
            wstr(", req=");       wdec64((uint64_t)request);
            wstr(") UNREADABLE da="); whex32((uint32_t)(uintptr_t)da);
            wstr(" in=");         whex32((uint32_t)(da ? da->inputBuffer : 0));
            wstr(" insz=");       wdec32(da ? da->inputBufferSize : 0);
            wstr("\n");
        }
    }

    return real_ioctl(fd, request, arg);
}
