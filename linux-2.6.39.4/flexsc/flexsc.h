#ifndef __SRC_FLEXSC_FLEXSC_H__
#define __SRC_FLEXSC_FLEXSC_H__

#include <linux/linkage.h>
#include <linux/cpumask.h>
#include <linux/syscalls.h>
#include <linux/spinlock.h>
#include <linux/threads.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/bug.h>
#include <linux/atomic.h>
#include <linux/mman.h>
#include <linux/smp.h>

#include <linux/flexsc.h>

#include <asm/processor.h>

// #define FLEXSC_DEBUG

struct flexsc_sysentry;

#define MAX_NCPUS                       32
#define MAX_KTHREADS_GROUP              32
#define MAX_NFIBERS                     0x0800

#define CACHE_SIZE                      (SMP_CACHE_BYTES)
#define CACHE_MASK                      (SMP_CACHE_BYTES - 1)
#define __cacheline__                   ____cacheline_aligned_in_smp

#define STACKSIZE                       (1 * 1024 * 1024)

#define static_assert(x)                                    \
    do { switch (x) {case 0: ; case (x): ; } } while (0)

#define flexsc_panic(fmt, args ...)             \
    do {                                        \
        printk("flexsc panic: " fmt, ##args);   \
        BUG();                                  \
    } while (0)

#ifdef FLEXSC_DEBUG
#define flexsc_assert(x)                                    \
    do {                                                    \
        if (unlikely(!(x))) {                               \
            flexsc_panic("Assertion: `" #x "' Failed!\n");  \
        }                                                   \
    } while (0)
#else
#define flexsc_assert(x)
#endif

#define ERR_DEBUG                       400

#define ERR_INIT_AGAIN                  500
#define ERR_INIT_COPY_INFO              501
#define ERR_INIT_SAVE_INFO              502
#define ERR_INIT_INV_INFO               503
#define ERR_INIT_BAD_INFO               504
#define ERR_INIT_KTHREAD_GROUP          505
#define ERR_INIT_ALLOC_KSTRUCT          506
#define ERR_INIT_MAX_NFIBERS            507

#define ERR_START_CPU                   600
#define ERR_START_KSTRUCT               601
#define ERR_START_SYSPAGE               602
#define ERR_START_SYSPAGE_STATE         603
#define ERR_START_CPUMASK               604
#define ERR_START_SYSBOX_POOL           605

#define ERR_KTHREAD_KSTRUCT             700
#define ERR_KTHREAD_SYSPAGE             701
#define ERR_KTHREAD_SETCPU              702
#define ERR_KTHREAD_CPUMASK             703

#define ERR_SYSCALL_SYSNUM              800
#define ERR_SYSCALL_SYSADDR             801
#define ERR_SYSCALL_SYSPAGE             802
#define ERR_SYSCALL_SYSPAGE_STATE       803
#define ERR_SYSCALL_WAIT_SIZE           804

#define ERR_SERVE_CPU                   900
#define ERR_SERVE_CPULIST               901
#define ERR_SERVE_SIZE                  902
#define ERR_SERVE_KSTRUCT               903
#define ERR_SERVE_SYSPAGE               904
#define ERR_SERVE_SYSPAGE_STATE         905
#define ERR_SERVE_WORKER_CPU            906
#define ERR_SERVE_SYSPAGE_WORKER_STATE  907

struct flexsc_initinfo {
    unsigned long maxnfibers;
    unsigned long stackbase;
    unsigned long stacksize;
};

struct flexsc_sysentry {
    long sysargs[6];
    union {
        struct {
            unsigned int sysname;
        };
        struct {
            long sysret;
        };
    };
    unsigned long reserved;
} __cacheline__;

struct padding {
    char x[0];
} __cacheline__;

#define padding(x) struct padding __padding_##x

struct mbox_struct {
    volatile int recv;
    padding(0);
    volatile int send;
    padding(1);
    volatile struct flexsc_sysentry * pool[MAX_NFIBERS];
} __cacheline__;

#define STAT_WORK          0
#define STAT_EXIT          1
#define STAT_NONE          2

#define TYPE_DEFAULT       0
#define TYPE_SERVER        1
#define TYPE_WORKER        2

struct sysbox_pool {
    struct flexsc_sysentry ** wait_pool;
    struct flexsc_sysentry ** volatile _____pool;
    struct flexsc_sysentry ** volatile done_pool;
    struct mbox_struct * sendbox;
    struct mbox_struct * recvbox;
};

struct flexsc_syspage {
    struct list_head worker_list;
    struct task_struct * volatile waiter;

    volatile unsigned int worker;
    volatile unsigned char __stat;
             unsigned char __type;
    volatile unsigned char fork_flag;
             unsigned char __reserved0;

    volatile int wait_size;
    volatile int done_size;

    volatile int mbox_mbox;
             int mbox_used;

    struct flexsc_sysentry ** wait_pool;
    struct flexsc_sysentry ** volatile done_pool;
    struct flexsc_sysentry ** volatile _____pool;

    struct {
        struct mbox_struct * __sendbox;
        struct mbox_struct * __recvbox;
    } mbox_array[MAX_NCPUS];
    
    padding(0);
    struct flexsc_sysentry * __sysbox_pool[MAX_NFIBERS * 3];

    padding(1);
    struct mbox_struct __mbox_pool[MAX_NCPUS];

#ifdef FLEXSC_DEBUG
    padding(x);
    unsigned long mon_show;
    unsigned long mon_syscall[__NR_syscall_max];
    padding(y);
    volatile unsigned long mon_wait_step0;
    volatile unsigned long mon_wait_step1;
    unsigned long mon_wait_size[3];
    unsigned long mon_done_size[3];
    volatile int mon_reset;
    volatile int mon_wait_reset;
    unsigned long mon_mbox_size[6];
    unsigned long mon_ticks;
#endif
} __cacheline__;

struct flexsc_kstruct {
    struct flexsc_syspage *syspages;
    spinlock_t lock;
    atomic_t count;
    volatile short start;
    volatile short exit;
    volatile int exit_code;

    padding(0);
    unsigned long stackbase;
    unsigned long stacksize;
};

struct flexsc_kstruct *alloc_flexsc_kstruct(struct flexsc_initinfo *info);
void drop_flexsc_kstruct(struct flexsc_kstruct *kstruct, int exit_code);

#define wakeup_task(task) do { wake_up_process(task); } while (0)

#define link_to_task(link) ({ list_entry(link, struct task_struct, worker_link); })

int create_worker(struct flexsc_syspage *syspage, int cpu);

static __always_inline struct task_struct *
alloc_worker(struct flexsc_syspage *syspage) {
    struct list_head *list = &(syspage->worker_list), *next;
    if (likely((next = list->next) != list)) {
        list_del_init(next), syspage->worker ++;
        return link_to_task(next);
    }
    return NULL;
}

#define syspage_mayexit(syspage) ({ (syspage)->__stat != STAT_WORK; })

static __always_inline int
sysentry_check(struct flexsc_kstruct *kstruct, struct flexsc_sysentry *syscall) {
    unsigned long addr = (unsigned long)syscall;
    unsigned long stackbase = kstruct->stackbase, stacksize = kstruct->stacksize;
    if (likely(stackbase <= addr)) {
        unsigned long offset = addr - stackbase;
        if (offset <= stacksize - sizeof(struct flexsc_sysentry)) {
            if ((offset & (STACKSIZE - 1)) >= PAGE_SIZE) {
                return 0;
            }
        }
    }
    return -1;
}

typedef long (*sys_call_ptr_t)(long, long, long,
                               long, long, long);

static __always_inline long
do_syscall(unsigned int sysname, long sysargs[]) {
    extern const sys_call_ptr_t sys_call_table[];
    if (unlikely(sysname >= __SYSNUM_flexsc_base)) {
        return -ERR_SYSCALL_SYSNUM;
    }
    if (likely(sysname < __NR_syscall_max)) {
        return sys_call_table[sysname](sysargs[0], sysargs[1],
                                       sysargs[2], sysargs[3],
                                       sysargs[4], sysargs[5]);
    }
    return -ENOSYS;
}

#define set_worker_kstate(task)                     \
    do {                                            \
        (task)->worker_kstate = KSTATE_WORKING;     \
    } while (0)

#define clear_worker_kstate(task)                   \
    do {                                            \
        (task)->worker_kstate = KSTATE_NONE;        \
    } while (0)

static __always_inline int
check_cpumask(int pid, int __cpu) {
    struct cpumask mask;
    int ret, cpu;
    if ((ret = sched_getaffinity(pid, &mask)) != 0) {
        return ret;
    }
    for_each_cpu(cpu, &mask) {
        if (cpu != __cpu) {
            return -ERR_START_CPUMASK;
        }
    }
    return 0;
}

static __always_inline void
task_set_syspage(struct task_struct *task, struct flexsc_syspage *syspage) {
    INIT_LIST_HEAD(&(task->worker_link));
    task->worker_kstate = KSTATE_NONE;
    task->syspage = syspage;
}

void flexsc_start_hook(void);
void *flexsc_mmap(size_t size, int locked, int *errp);

int __flexsc_kdefault_main(struct task_struct *task);
int __flexsc_kserver_main(struct task_struct *task);
int __flexsc_kworker_main(struct task_struct *task);

#endif /* !__SRC_FLEXSC_FLEXSC_H__ */
