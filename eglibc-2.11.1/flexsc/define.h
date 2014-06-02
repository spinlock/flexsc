#ifndef __FLEXSC_DEFINE_H__
#define __FLEXSC_DEFINE_H__

#ifndef likely
#define likely(x)               __builtin_expect(!!(x), 1)
#endif

#ifndef unlikely
#define unlikely(x)             __builtin_expect(!!(x), 0)
#endif

#ifndef __always_inline
#define __always_inline         __attribute__((always_inline)) inline
#endif

#ifndef __always_noinline
#define __always_noinline       __attribute__((always_noinline))
#endif

#ifndef __noreturn
#define __noreturn              __attribute__((__noreturn__))
#endif

#define static_assert(x)                                    \
    do { switch (x) { case 0: ; case (x): ; } } while (0)

#define to_struct(ptr, type, member)                    \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define CACHE_SIZE              64
#define CACHE_MASK              (CACHE_SIZE - 1)
#define __cacheline__           __attribute__((__aligned__(CACHE_SIZE)))

struct __padding {
    char x[0];
} __cacheline__;

#define padding(x) struct __padding __padding_##x##__

#endif /* !__FLEXSC_DEFINE_H__ */
