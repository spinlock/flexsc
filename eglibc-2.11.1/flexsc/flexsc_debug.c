#include <stdarg.h>
#include <sys/types.h>
#include <flexsc.h>
#include "flexsc.h"

static spinlock_t flexsc_stdio_lock = SPIN_INIT;

int __flexsc_vsnprintf(char *s_buf, size_t s_size, const char *fmt, va_list ap);

static void
__flexsc_debug_sub(int need_lock, const char *buf, size_t len) {
    if (need_lock) {
        flexsc_debug_lock();
    }

    register long arg0 __asm__ ("rdi") = (long)buf;
    register long arg1 __asm__ ("rsi") = (long)len;

    __asm__ __volatile__ ("syscall"
                          :: "a" (__NR_flexsc_debug), "r" (arg0), "r" (arg1)
                          : "memory", "cc", "r11", "cx");

    if (need_lock) {
        flexsc_debug_unlock();
    }
}

void
flexsc_debug(const char *fmt, ...) {
    char s_buf[4096];
    va_list arg;
    int done;
    va_start(arg, fmt);
    done = __flexsc_vsnprintf(s_buf, sizeof(s_buf), fmt, arg);
    va_end(arg);
    if (done > 0) {
        __flexsc_debug_sub(1, s_buf, done);
    }
}

void
flexsc_debug_nolock(const char *fmt, ...) {
    char s_buf[4096];
    va_list arg;
    int done;
    va_start(arg, fmt);
    done = __flexsc_vsnprintf(s_buf, sizeof(s_buf), fmt, arg);
    va_end(arg);
    if (done > 0) {
        __flexsc_debug_sub(0, s_buf, done);
    }
}

void
flexsc_debug_lock(void) {
    spin_lock(&flexsc_stdio_lock);
}

void
flexsc_debug_unlock(void) {
    spin_unlock(&flexsc_stdio_lock);
}
