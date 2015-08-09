#ifdef DEBUG
#include <stdarg.h>
#include <stdio.h>
#endif
#include <spu_mfcio.h>

// Necessary setup for __linux_syscall() - based on those in newlib

#define __NR_ioctl 54

/* System callbacks from the SPU. See kernel source file
 *    arch/powerpc/include/asm/spu.h.  */
struct spu_syscall_block
{
    unsigned long long nr_ret;    /* System call nr and return value.  */
    unsigned long long parm[6];   /* System call arguments.  */
};

int __linux_syscall (struct spu_syscall_block *s);

int ioctl(unsigned int d, unsigned int request, unsigned long a) {
    struct spu_syscall_block s = {
        __NR_ioctl,
        { d, request, a, 0, 0, 0 }
    };
    return __linux_syscall(&s);
}

int ioctl_eaddr(unsigned int d, unsigned int request, void* lsa, void* ea, int size) {
    int r = ioctl(d, request, (uint32_t)ea);
    spu_mfcdma32(lsa, (unsigned long)ea, size, 0, MFC_GET_CMD);
    mfc_write_tag_mask(1); spu_mfcstat(MFC_TAG_UPDATE_ALL);
    return r;
}

#ifdef DEBUG
int __nldbl_printf(const char *format, ...) {
    va_list va_args;
    va_start(va_args, format);
    int r = vprintf(format, va_args);
    va_end(va_args);
    return r;
}
#endif
