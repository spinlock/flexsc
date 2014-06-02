#ifndef __FLEXSC_FLEXSC_H__
#define __FLEXSC_FLEXSC_H__

#include <stddef.h>
#include "define.h"
#include "assert.h"

typedef struct {
    unsigned int slock;
} spinlock_t;

#define SPIN_INIT {0}

static __always_inline spinlock_t *
spin_init(spinlock_t *lock) {
    lock->slock = 0;
    return lock;
}

static __always_inline void
spin_lock(spinlock_t *lock) {
    short inc = 0x0100;
    __asm__ __volatile__ ("  lock; xaddw %w0, %1;"
                          "1:"
                          "  cmpb %h0, %b0;"
                          "  je 2f;"
                          "  rep ; nop;"
                          "  movb %1, %b0;"
                          "  jmp 1b;"
                          "2:"
                          : "+Q" (inc), "+m" (lock->slock)
                          :: "memory", "cc");
}

static __always_inline void
spin_unlock(spinlock_t *lock) {
    __asm__ __volatile__("lock; incb %0;" : "+m" (lock->slock) :: "memory", "cc");
}

static inline void
cpu_relax(void) {
    __asm__ __volatile__ ("rep; nop;");
}

static inline void
cpu_barrier(void) {
    __asm__ __volatile__ ("mfence" ::: "memory");
}

#define __syscall_XX(sysnum, arg0, arg1, arg2, arg3, arg4, arg5) ({     \
            int __sysnum = (int)(sysnum);                               \
            register long __arg0 __asm__ ("rdi") = (long)(arg0);        \
            register long __arg1 __asm__ ("rsi") = (long)(arg1);        \
            register long __arg2 __asm__ ("rdx") = (long)(arg2);        \
            register long __arg3 __asm__ ("r10") = (long)(arg3);        \
            register long __arg4 __asm__ ("r8")  = (long)(arg4);        \
            register long __arg5 __asm__ ("r9")  = (long)(arg5);        \
                                                                        \
            long __ret;                                                 \
            __asm__ __volatile__ ("syscall"                             \
                                  : "=a" (__ret)                        \
                                  : "0" (__sysnum),                     \
                                    "r" (__arg0), "r" (__arg1),         \
                                    "r" (__arg2), "r" (__arg3),         \
                                    "r" (__arg4), "r" (__arg5)          \
                                  : "memory", "cc", "r11", "cx");       \
            __ret;                                                      \
        })

#define syscall6(sysnum, arg0, arg1, arg2, arg3, arg4, arg5)            \
    __syscall_XX(sysnum, arg0, arg1, arg2, arg3, arg4, arg5)
#define syscall5(sysnum, arg0, arg1, arg2, arg3, arg4)                  \
    __syscall_XX(sysnum, arg0, arg1, arg2, arg3, arg4, 0)
#define syscall4(sysnum, arg0, arg1, arg2, arg3)                        \
    __syscall_XX(sysnum, arg0, arg1, arg2, arg3, 0, 0)
#define syscall3(sysnum, arg0, arg1, arg2)                              \
    __syscall_XX(sysnum, arg0, arg1, arg2, 0, 0, 0)
#define syscall2(sysnum, arg0, arg1)                                    \
    __syscall_XX(sysnum, arg0, arg1, 0, 0, 0, 0)
#define syscall1(sysnum, arg0)                                          \
    __syscall_XX(sysnum, arg0, 0, 0, 0, 0, 0)
#define syscall0(sysnum)                                                \
    __syscall_XX(sysnum, 0, 0, 0, 0, 0, 0)

struct flexsc_sysentry;

typedef long (*flexsc_syscall_t)(struct flexsc_sysentry *entry, unsigned int sysnum);

extern void flexcs_debug(const char *fmt, ...);
extern void flexsc_debug_nolock(const char *fmt, ...);
extern void flexsc_debug_lock(void);
extern void flexsc_debug_unlock(void);

extern int flexsc_enabled(void);

extern unsigned int flexsc_current_fid(void);

#define ERR_FLEXSC_NOFIB                400
#define ERR_FLEXSC_INV_GID              401
#define ERR_FLEXSC_INV_RID              402
#define ERR_FLEXSC_INV_WAIT             403
#define ERR_FLEXSC_INV_DETACH           404

#define __NR_flexsc_init                350
#define __NR_flexsc_debug               351
#define __NR_flexsc_start               352
#define __NR_flexsc_serve               353
#define __NR_flexsc_syscall             354

struct fiber_struct;

struct flexsc_sysentry {
    union {
        struct {
            long sysargs[6];
            union {
                struct {
                    unsigned int sysnum;
                };
                struct {
                    long sysret;
                };
            };
        };
        struct {
            struct flexsc_sysentry *next;
        };
    };
    struct fiber_struct *owner;
} __cacheline__;

#define MAX_NCPUS                       64
#define MAX_CPUINFO_SIZE                64

struct cpuinfo {
    int list[MAX_CPUINFO_SIZE];
    int size;
};

#endif /* !__FLEXSC_FLEXSC_H__ */
