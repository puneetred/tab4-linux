/* logshim.c — LD_PRELOAD shim for bionic liblog entry points.
 * The MRVL blobs call __android_log_print etc. via liblog's PLT; on this
 * device there is no logd socket so traces are lost. This shim prints them
 * to stdout. Build as a shared library, push to /system/lib, run with
 * LD_PRELOAD=/system/lib/logshim.so.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void emit(const char* tag, const char* msg)
{
    char buf[1024];
    int n = 0;
    if (tag) {
        while (*tag && n < 1000) buf[n++] = *tag++;
        buf[n++] = ':';
        buf[n++] = ' ';
    }
    const char* s = msg;
    while (*s && n < 1023) buf[n++] = *s++;
    buf[n++] = '\n';
    write(1, buf, n);
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    emit(tag, buf);
    return 0;
}

int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap)
{
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    emit(tag, buf);
    return 0;
}

int __android_log_write(int prio, const char* tag, const char* msg)
{
    emit(tag, msg);
    return 0;
}

int __android_log_buf_write(int bufID, int prio, const char* tag, const char* msg)
{
    emit(tag, msg);
    return 0;
}

int __android_log_buf_print(int bufID, int prio, const char* tag, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    emit(tag, buf);
    return 0;
}

void __android_log_assert(const char* cond, const char* tag, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    emit(tag, buf);
    abort();
}