/* galcore-test.c — Stage 3: libGAL round-trip on the device.
 * Bionic-linked executable: dlopen libGAL.so, gcoHAL_Construct, query chip.
 * No stdio: prints via raw write() so it works without bionic crt init.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

typedef int32_t gceSTATUS;
#define gcvSTATUS_OK 0

typedef struct {
    uint32_t chipModel;
    uint32_t chipRevision;
    uint32_t chipFeatures;
    uint32_t chipMinorFeatures;
    uint32_t chipMinorFeatures1;
    uint32_t chipMinorFeatures2;
    uint32_t chipMinorFeatures3;
    uint32_t chipMinorFeatures4;
} gcsHAL_QUERY_CHIP_IDENTITY_8;

typedef struct {
    uint32_t internalPhysical;
    uint32_t internalSize;
    uint32_t externalPhysical;
    uint32_t externalSize;
    uint32_t contiguousPhysical;
    uint32_t contiguousSize;
} gcsHAL_QUERY_VIDEO_MEMORY;

typedef void* gcoHAL;
typedef void* gcoOS;
typedef gceSTATUS (*fn_gcoHAL_Construct)(int32_t os, int32_t hwType, gcoHAL* Hal);
typedef gceSTATUS (*fn_gcoHAL_QueryChipIdentity)(int32_t unused, gcsHAL_QUERY_CHIP_IDENTITY_8* Identity, int32_t a, int32_t b, int32_t c);
typedef gceSTATUS (*fn_gcoHAL_QueryVideoMemory)(int32_t unused, int32_t* internalPhysical, int32_t* internalSize, int32_t* externalPhysical, int32_t* externalSize, int32_t* contiguousPhysical, int32_t* contiguousSize);
typedef gceSTATUS (*fn_gcoHAL_Destroy)(gcoHAL Hal);
typedef gceSTATUS (*fn_gcoOS_ModuleConstructor)(int32_t processID);
typedef gceSTATUS (*fn_gcoOS_Destroy)(gcoOS OS);

static void out(const char* s)
{
    int l = 0;
    while (s[l]) l++;
    asm volatile(
        "mov r7, #4\n"   /* __NR_write */
        "mov r0, #1\n"
        "mov r1, %0\n"
        "mov r2, %1\n"
        "svc 0\n"
        :: "r"(s), "r"(l) : "r7", "r0", "r1", "r2", "memory");
}

static void hex32(const char* tag, uint32_t v)
{
    char b[64];
    int i = 0;
    static const char* hx = "0123456789abcdef";
    const char* t = tag;
    while (*t) b[i++] = *t++;
    b[i++] = '=';
    b[i++] = '0';
    b[i++] = 'x';
    for (int sh = 28; sh >= 0; sh -= 4) b[i++] = hx[(v >> sh) & 0xf];
    b[i++] = '\n';
    asm volatile(
        "mov r7, #4\n"
        "mov r0, #1\n"
        "mov r1, %0\n"
        "mov r2, %1\n"
        "svc 0\n"
        :: "r"(b), "r"(i) : "r7", "r0", "r1", "r2", "memory");
}

static void dec32(const char* tag, uint32_t v)
{
    char b[64];
    int i = 0;
    const char* t = tag;
    while (*t) b[i++] = *t++;
    b[i++] = '=';
    char rev[16];
    int n = 0;
    if (v == 0) rev[n++] = '0';
    while (v) { rev[n++] = '0' + (v % 10); v /= 10; }
    while (n) b[i++] = rev[--n];
    b[i++] = '\n';
    asm volatile(
        "mov r7, #4\n"
        "mov r0, #1\n"
        "mov r1, %0\n"
        "mov r2, %1\n"
        "svc 0\n"
        :: "r"(b), "r"(i) : "r7", "r0", "r1", "r2", "memory");
}

static const char* status_name(int32_t s)
{
    static const char* names[] = {
        "OK", "NOT_ALIGNED", "INVALID_ADDRESS", "INVALID_ARGUMENT", "OUT_OF_RESOURCES",
        "NO_MEMORY", "ALREADY_ALLOCATED", "NOT_SUPPORTED", "VERSION_MISMATCH", "MISSING_2D",
        "TIMEOUT", "NOT_INITIALIZED", "INTERRUPTED", "CANNOT_LOCK", "DISABLED",
        "OUT_OF_MEMORY", "BUFFER_TOO_SMALL", "NOT_FOUND", "NOT_ENOUGH_SPACE", "INVALID_DATA",
        "MAX_QUEUED", "NOT_READY", "NOT_ENOUGH_DESCRIPTORS", "NOT_OWNER", "INTERRUPTED_RESET",
    };
    if ((uint32_t)s < sizeof(names)/sizeof(names[0])) return names[s];
    return "?";
}

static void status(const char* tag, int32_t s)
{
    out(tag);
    out(" -> ");
    dec32("status", (uint32_t)s);
    out(tag);
    out(" -> ");
    out(status_name(s));
    out("\n");
}

int main(void)
{
    out("== galcore-test: bionic libGAL round-trip ==\n");
    void* h = dlopen("/system/lib/libGAL.so", RTLD_NOW);
    if (!h) {
        out("dlopen /system/lib/libGAL.so FAILED\n");
        return 1;
    }
    out("dlopen OK\n");

    fn_gcoHAL_Construct ctor = (fn_gcoHAL_Construct)dlsym(h, "gcoHAL_Construct");
    fn_gcoHAL_QueryChipIdentity qci = (fn_gcoHAL_QueryChipIdentity)dlsym(h, "gcoHAL_QueryChipIdentity");
    fn_gcoHAL_QueryVideoMemory qvm = (fn_gcoHAL_QueryVideoMemory)dlsym(h, "gcoHAL_QueryVideoMemory");
    fn_gcoHAL_Destroy dtor = (fn_gcoHAL_Destroy)dlsym(h, "gcoHAL_Destroy");
    fn_gcoOS_ModuleConstructor osCtor = (fn_gcoOS_ModuleConstructor)dlsym(h, "gcoOS_ModuleConstructor");
    fn_gcoOS_Destroy osDtor = (fn_gcoOS_Destroy)dlsym(h, "gcoOS_Destroy");
    if (!ctor || !qci || !qvm || !dtor || !osCtor || !osDtor) {
        out("dlsym FAIL: ");
        hex32("ctor", (uint32_t)ctor);
        hex32("qci", (uint32_t)qci);
        hex32("qvm", (uint32_t)qvm);
        hex32("dtor", (uint32_t)dtor);
        hex32("osCtor", (uint32_t)osCtor);
        hex32("osDtor", (uint32_t)osDtor);
        return 1;
    }
    out("dlsym OK\n");

    gceSTATUS s = osCtor(getpid());
    status("gcoOS_ModuleConstructor", s);
    if (s != gcvSTATUS_OK) return 2;

    gcoHAL hal = NULL;
    s = ctor(0, 0, &hal);
    status("gcoHAL_Construct", s);
    hex32("hal", (uint32_t)hal);
    if (s != gcvSTATUS_OK) { osDtor(0); return 3; }

    gcsHAL_QUERY_CHIP_IDENTITY_8 id;
    s = qci(0, &id, 0, 0, 0);
    status("gcoHAL_QueryChipIdentity", s);
    if (s == gcvSTATUS_OK) {
        hex32("chipModel", id.chipModel);
        hex32("chipRevision", id.chipRevision);
        hex32("chipFeatures", id.chipFeatures);
        hex32("chipMinorFeatures", id.chipMinorFeatures);
        hex32("chipMinorFeatures1", id.chipMinorFeatures1);
        hex32("chipMinorFeatures2", id.chipMinorFeatures2);
        hex32("chipMinorFeatures3", id.chipMinorFeatures3);
        hex32("chipMinorFeatures4", id.chipMinorFeatures4);
    }

    gcsHAL_QUERY_VIDEO_MEMORY vm;
    s = qvm(0, &vm.internalPhysical, &vm.internalSize, &vm.externalPhysical,
            &vm.externalSize, &vm.contiguousPhysical, &vm.contiguousSize);
    status("gcoHAL_QueryVideoMemory", s);
    if (s == gcvSTATUS_OK) {
        hex32("internalPhysical", vm.internalPhysical);
        dec32("internalSize", vm.internalSize);
        hex32("externalPhysical", vm.externalPhysical);
        dec32("externalSize", vm.externalSize);
        hex32("contiguousPhysical", vm.contiguousPhysical);
        dec32("contiguousSize", vm.contiguousSize);
    }

    s = dtor(hal);
    status("gcoHAL_Destroy", s);

    out("-- raw GPL ioctl (kernel ground truth) --\n");
    int rawfd = 3;
    {
        struct { void* in; uint32_t inSize; void* out; uint32_t outSize; } args;
        uint8_t iface[160];
        for (int cmd = 0; cmd <= 1; cmd++) {
            memset(iface, 0, sizeof(iface));
            iface[0] = cmd;
            args.in = iface; args.inSize = sizeof(iface);
            args.out = iface; args.outSize = sizeof(iface);
            int rc;
            asm volatile(
                "mov r7, #54\n"
                "mov r0, %2\n"
                "mov r1, #0x7530\n"
                "mov r2, %3\n"
                "svc 0\n"
                "mov %0, r0\n"
                : "=r"(rc)
                : "0"(0), "r"(rawfd), "r"(&args)
                : "r7", "r1", "r2", "memory");
            out(cmd ? "cmd=1 QUERY_CHIP_IDENTITY: " : "cmd=0 QUERY_VIDEO_MEMORY: ");
            hex32("status", *(uint32_t*)(iface + 4));
            for (int i = 0; i < 8; i++) {
                hex32("reply", *(uint32_t*)(iface + 8 + i * 4));
            }
            out("rawrc=");
            dec32("rc", (uint32_t)rc);
        }
    }
    out("== done ==\n");
    return 0;
}
