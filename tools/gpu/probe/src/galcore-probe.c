/*
 * galcore-probe.c — Stage-1 kernel contract probe for /dev/galcore.
 *
 * Opens /dev/galcore and exercises the Vivante kernel ABI directly with
 * the ioctl codes and gcsHAL_INTERFACE struct from the GPL kernel source
 * (drivers/marvell/graphics/galcore_4x/hal/inc/gc_hal_driver.h).
 *
 * Queries:
 *   1. gcvHAL_QUERY_CHIP_IDENTITY  — chip model, revision, features
 *   2. gcvHAL_VERSION              — kernel userspace-ABI version handshake
 *   3. gcvHAL_QUERY_VIDEO_MEMORY   — internal/external/contiguous pools
 *
 * Build (cross, static, for the device):
 *   arm-linux-gnueabihf-gcc -static -I../inc \
 *       -o galcore-probe galcore-probe.c
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdint.h>

#include "gc_hal_base.h"
#include "gc_hal_driver.h"

/* Kernel driver wrapper: ioctl(fd, 30000, &DRIVER_ARGS). Defined in the
 * kernel source's gc_hal_kernel_os.h. */
typedef struct _DRIVER_ARGS
{
    void    *InputBuffer;
    uint32_t InputBufferSize;
    void    *OutputBuffer;
    uint32_t OutputBufferSize;
} DRIVER_ARGS;

#define DEV_GALCORE "/dev/galcore"

static int galioctl(int fd, unsigned long code, gcsHAL_INTERFACE *iface)
{
    DRIVER_ARGS args;
    int ret;

    iface->status = gcvSTATUS_OK;

    memset(&args, 0, sizeof(args));
    args.InputBuffer     = iface;
    args.InputBufferSize = sizeof(*iface);
    args.OutputBuffer    = iface;
    args.OutputBufferSize = sizeof(*iface);

    ret = ioctl(fd, code, &args);
    if (ret < 0)
        return -errno;

    if (iface->status != gcvSTATUS_OK)
        return -iface->status;

    return 0;
}

int main(void)
{
    int fd;
    int ret;

    fd = open(DEV_GALCORE, O_RDWR);
    if (fd < 0) {
        printf("FAIL: open(%s): %s\n", DEV_GALCORE, strerror(errno));
        return 1;
    }
    printf("open(%s) ok (fd=%d)\n", DEV_GALCORE, fd);

    /* ---- 1. Chip identity ---- */
    {
        gcsHAL_INTERFACE iface;
        gcsHAL_QUERY_CHIP_IDENTITY *id = &iface.u.QueryChipIdentity;

        memset(&iface, 0, sizeof(iface));
        iface.command = gcvHAL_QUERY_CHIP_IDENTITY;
        iface.hardwareType = gcvHARDWARE_3D;

        ret = galioctl(fd, IOCTL_GCHAL_INTERFACE, &iface);
        if (ret < 0) {
            printf("CHIP_IDENTITY: FAIL (%s, status=0x%x)\n",
                   strerror(-ret), iface.status);
        } else {
            printf("CHIP_IDENTITY: ok\n");
            printf("  chipModel            = 0x%08x\n", id->chipModel);
            printf("  chipRevision         = 0x%08x\n", id->chipRevision);
            printf("  chipFeatures         = 0x%08x\n", id->chipFeatures);
            printf("  chipMinorFeatures    = 0x%08x\n", id->chipMinorFeatures);
            printf("  chipMinorFeatures1   = 0x%08x\n", id->chipMinorFeatures1);
            printf("  streamCount          = %u\n", id->streamCount);
            printf("  registerMax          = %u\n", id->registerMax);
            printf("  threadCount          = %u\n", id->threadCount);
            printf("  shaderCoreCount      = %u\n", id->shaderCoreCount);
            printf("  vertexCacheSize      = %u\n", id->vertexCacheSize);
            printf("  pixelPipes           = %u\n", id->pixelPipes);
            printf("  instructionCount     = %u\n", id->instructionCount);
            printf("  numConstants         = %u\n", id->numConstants);
            printf("  varyingsCount        = %u\n", id->varyingsCount);
        }
    }

    /* ---- 2. Kernel/userspace version handshake ---- */
    {
        gcsHAL_INTERFACE iface;
        struct _gcsHAL_VERSION *v = &iface.u.Version;

        memset(&iface, 0, sizeof(iface));
        iface.command = gcvHAL_VERSION;
        iface.hardwareType = gcvHARDWARE_3D;

        ret = galioctl(fd, IOCTL_GCHAL_INTERFACE, &iface);
        if (ret < 0) {
            printf("VERSION: FAIL (%s, status=0x%x)\n",
                   strerror(-ret), iface.status);
        } else {
            printf("VERSION: ok -> kernel galcore %d.%d.%d (build %u)\n",
                   v->major, v->minor, v->patch, v->build);
        }
    }

    /* ---- 3. Video memory pools ---- */
    {
        gcsHAL_INTERFACE iface;
        struct _gcsHAL_QUERY_VIDEO_MEMORY *vm = &iface.u.QueryVideoMemory;

        memset(&iface, 0, sizeof(iface));
        iface.command = gcvHAL_QUERY_VIDEO_MEMORY;
        iface.hardwareType = gcvHARDWARE_3D;

        ret = galioctl(fd, IOCTL_GCHAL_INTERFACE, &iface);
        if (ret < 0) {
            printf("VIDEO_MEMORY: FAIL (%s, status=0x%x)\n",
                   strerror(-ret), iface.status);
        } else {
            printf("VIDEO_MEMORY: ok\n");
            printf("  internal    phys=0x%08llx size=%llu\n",
                   (unsigned long long)vm->internalPhysical,
                   (unsigned long long)vm->internalSize);
            printf("  external    phys=0x%08llx size=%llu\n",
                   (unsigned long long)vm->externalPhysical,
                   (unsigned long long)vm->externalSize);
            printf("  contiguous  phys=0x%08llx size=%llu\n",
                   (unsigned long long)vm->contiguousPhysical,
                   (unsigned long long)vm->contiguousSize);
        }
    }

    close(fd);
    printf("probe done\n");
    return 0;
}
