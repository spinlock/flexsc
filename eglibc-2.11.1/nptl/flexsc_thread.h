#ifndef __NPTL_FLEXSC_THREAD_H__
#define __NPTL_FLEXSC_THREAD_H__

#include "flexsc.h"

struct flexsc_sysentry;

#define MAX_NFIBERS            0x0800

struct mbox_struct {
    volatile int recv;
    padding(0);
    volatile int send;
    padding(1);
    volatile struct flexsc_sysentry * pool[MAX_NFIBERS];
} __cacheline__;

struct sysbox_pool {
    struct flexsc_sysentry ** wait_pool;
    struct flexsc_sysentry ** volatile done_pool;
    struct flexsc_sysentry ** volatile _____pool;
    struct mbox_struct * sendbox;
    struct mbox_struct * recvbox;
};

struct thread_struct {
    struct thread_struct *thread_array;
    int id, fork;
    int cpu, online_ncpus;
    volatile int boot;
    padding(0);
    struct fiber_struct * volatile current;
    struct fiber_struct * syscall;
    struct flexsc_sysentry ** wait_pool;
    struct flexsc_sysentry ** volatile done_pool;

#define MBOX_RATE_MAX 16

    volatile int wait_size;
    volatile int done_size;
             int mbox_rest;
             short mbox_rate;
             short fork_flag;

    struct mbox_struct * sendbox;
    struct mbox_struct * recvbox;

    struct flexsc_sysentry ** volatile _____pool;
    long syscall_counts;
    padding(1);
    spinlock_t fork_lock;
    struct flexsc_sysentry *fork_head;
} __cacheline__;

extern __thread struct thread_struct * __this;

void flexsc_thread_init(int cpuinfo_ncpus, struct cpuinfo *cpuinfo_list);

void schedule_exit(struct thread_struct *this, struct fiber_struct *fiber);
void schedule_fork(struct thread_struct *this, struct fiber_struct *fiber, struct flexsc_sysentry *entry);

#define thread_set_current(this, fiber)                                 \
    do {                                                                \
        THREAD_SETMEM(THREAD_SELF, flexsc_owner, (fiber));              \
        (this)->current = (fiber);                                      \
    } while (0)

static __always_inline unsigned long
rdtsc(void) {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi) :: "memory");
    return (unsigned long)hi << 32 | lo;
}

long flexsc_nptl_syscall(struct flexsc_sysentry *entry, unsigned int sysnum);

#endif /* !__NPTL_FLEXSC_THREAD_H__ */
