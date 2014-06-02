#include "flexsc.h"

#define BUFSIZE         127

static int
flexsc_debug(char __user *ptr, size_t __len) {
    char s_buf[BUFSIZE + 1];
    int err, len;
    while (__len != 0) {
        if ((len = __len) > BUFSIZE) {
            len = BUFSIZE;
        }
        if ((err = copy_from_user(s_buf, ptr, len)) != 0) {
            return -ERR_DEBUG;
        }
        __len -= len, ptr += len, s_buf[len] = '\0';
        printk("%s", s_buf);
    }
    return 0;
}

asmlinkage long
sys_flexsc_debug(char __user *ptr, size_t len) {
    return flexsc_debug(ptr, len);
}

DEFINE_SPINLOCK(__flexsc_test_lock);

asmlinkage void
sys_flexsc_test_lock(void) {
    int i;
    spin_lock(&__flexsc_test_lock);
    for (i = 0; i < 10; i ++) {
        cpu_relax();
    }
    spin_unlock(&__flexsc_test_lock);

    for (i = 0; i < 200; i ++) {
        cpu_relax();
    }
}
