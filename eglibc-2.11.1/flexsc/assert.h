#ifndef __FLEXSC_ASSERT_H__
#define __FLEXSC_ASSERT_H__

#include <sys/syscall.h>
#include "define.h"

// #define FLEXSC_DEBUG

static __always_inline void __attribute__((noreturn))
__flexsc_panic(void) {
    __asm__ __volatile__ ("syscall;"
                          :: "a" (__NR_exit_group), "D" (-1));
    while (1);
}

extern void flexsc_debug(const char *fmt, ...);

#define flexsc_panic(fmt, args ...)                                 \
    do {                                                            \
        flexsc_debug("flexsc panic: %s:%d: " fmt "\n",              \
                      __FILE__, __LINE__, ##args);                  \
        __flexsc_panic();                                           \
    } while (0)

#ifndef FLEXSC_DEBUG
#  define flexsc_assert(x)
#else
#  define flexsc_assert(x)                                  \
    do {                                                    \
        if (unlikely(!(x))) {                               \
            flexsc_panic("Assertion: `" #x "' Failed!\n");  \
        }                                                   \
    } while (0)
#endif /* !FLEXSC_DEBUG */

extern int flexsc_enabled(void);

#define flexsc_check_enabled(...)                           \
    do {                                                    \
        if(unlikely(flexsc_enabled())) {                    \
            flexsc_panic("check failed "__VA_ARGS__);       \
        }                                                   \
    } while (0)

#endif /* !__FLEXSC_ASSERT_H__ */
