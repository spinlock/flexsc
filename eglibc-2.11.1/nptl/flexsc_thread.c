#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include "flexsc.h"

__thread struct thread_struct * __this;

static volatile int __idle_main_mbox_rate = -1;

void
flexsc_set_mbox_rate(int rate) {
    flexsc_assert(rate >= 0 && rate <= MBOX_RATE_MAX);
    __idle_main_mbox_rate = rate;
}

static void
__init_thread_struct(struct thread_struct *this) {
    static_assert(sizeof(struct sysbox_pool) == 40);
    this->thread_array = NULL;
    this->id = this->fork = -1;
    this->cpu = -1;
    this->online_ncpus = 1;
    this->boot = 0;

    this->current = NULL;
    this->syscall = NULL;
    this->wait_pool = NULL;
    this->done_pool = NULL;

    this->wait_size = 0;
    this->done_size = 0;
    this->mbox_rest = 0;
    this->mbox_rate = 0;
    this->fork_flag = 0;

    this->sendbox = NULL;
    this->recvbox = NULL;

    this->_____pool = NULL;
    this->syscall_counts = 0;

    spin_init(&(this->fork_lock));
    this->fork_head = NULL;
}

static struct fiber_struct *
__init_alloc_fiber_main(struct thread_struct *this) {
    struct fiber_struct *fiber;
    if ((fiber = alloc_fiber()) != NULL) {
        fiber->type = FIBER_TYPE_MAIN;
    }
    return fiber;
}

static struct fiber_struct *
__init_alloc_fiber_syscall(struct thread_struct *this) {
    struct fiber_struct *fiber;
    if ((fiber = alloc_fiber()) != NULL) {
        flexsc_assert(this->syscall == NULL);
        fiber->type = FIBER_TYPE_SYSCALL;
        this->syscall = fiber;
    }
    return fiber;
}

static __always_inline void
start_kernel(void) {
    struct thread_struct *this = __this;
    struct sysbox_pool __pool, *pool = memset(&__pool, 0, sizeof(__pool));
   
    syscall2(__NR_flexsc_start, this->cpu, pool);

    if (pool->wait_pool == NULL || pool->done_pool == NULL || pool->_____pool == NULL) {
        flexsc_panic("invalid pool: addr %016lx %016lx %016lx %016lx %016lx\n",
                     (long)pool->wait_pool, (long)pool->done_pool,
                     (long)pool->_____pool,
                     (long)pool->sendbox, (long)pool->recvbox);
    }
    size_t size = sizeof(struct flexsc_sysentry *) * MAX_NFIBERS;
    flexsc_debug_nolock("glibc %3d %016lx %016lx %016lx %016lx %016lx %016lx\n", this->cpu, size,
                        (long)(pool->wait_pool), (long)(pool->done_pool),
                        (long)(pool->_____pool),
                        (long)(pool->sendbox), (long)(pool->recvbox));

    flexsc_assert((pool->sendbox == NULL && pool->recvbox == NULL) ||
                  (pool->sendbox != NULL && pool->recvbox != NULL));

    this->wait_pool = pool->wait_pool;
    this->done_pool = pool->done_pool;
    this->_____pool = pool->_____pool;
    this->sendbox = pool->sendbox, this->recvbox = this->recvbox;
}

static void
__bind_to_cpu(int cpu) {
    int ret;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu, &mask);
    if ((ret = sched_setaffinity(0, sizeof(cpu_set_t), &mask)) != 0) {
        flexsc_panic("init: bind cpu[%d] failed.\n", cpu);
    }
    flexsc_assert(sched_getcpu() == cpu);
}

extern void __noreturn __fiber_clone(void *(*__fn)(void *arg), void *arg);
void __noreturn syscall_main(void);

static void
__boot_main_cpu(struct thread_struct *this) {
    __bind_to_cpu(this->cpu);
    __this = this;

    struct fiber_struct *fiber;
    if ((fiber = __init_alloc_fiber_main(this)) == NULL) {
        flexsc_panic("init: cannot alloc main fiber.\n");
    }
    fiber_set_state(fiber, FIBER_STATE_RUNNABLE);
    thread_set_current(this, fiber);
    
    cpu_barrier();
    this->boot = 1;
    
    if ((fiber = __init_alloc_fiber_syscall(this)) == NULL) {
        flexsc_panic("init: cannot alloc syscall fiber[%d].\n", this->id);
    }
    
    unsigned long *stacktop = (unsigned long *)fiber->stacktop - 4;
    stacktop[0] = (unsigned long)__fiber_clone;
    stacktop[1] = (unsigned long)fiber;
    stacktop[2] = (unsigned long)syscall_main;

    struct fiber_context *context = &(fiber->context);
    context->rsp = (unsigned long)stacktop;
    
    fiber_set_state(fiber, FIBER_STATE_MONITOR);
}

static void
__thread_main(struct thread_struct *this) {
    __bind_to_cpu(this->cpu);
    __this = this;

    struct fiber_struct *fiber;
    if ((fiber = __init_alloc_fiber_syscall(this)) == NULL) {
        flexsc_panic("init: cannot alloc syscall fiber[%d].\n", this->id);
    }
    
    fiber_set_state(fiber, FIBER_STATE_MONITOR);
    thread_set_current(this, fiber);
    
    cpu_barrier();
    this->boot = 1;
    
    start_kernel();

    while (!flexsc_enabled()) {
        cpu_relax();
    }

    syscall_main();
}

static void
__boot_other_cpu(struct thread_struct *this) {
    pthread_t tid;
    int ret;
    if ((ret = pthread_create(&tid, NULL, (void *)__thread_main, this)) != 0) {
        flexsc_panic("init: boot cpu[%d] failed. %d:%s.\n",
                     this->id, ret, strerror(ret));
    }
}

void
flexsc_thread_init(int cpuinfo_ngroups, struct cpuinfo *cpuinfo_list) {
    static_assert(sizeof(struct thread_struct) % CACHE_SIZE == 0);

    flexsc_assert(cpuinfo_ngroups > 0 && cpuinfo_ngroups <= MAX_NCPUS && cpuinfo_list != NULL);

    struct cpuinfo *cpuinfo;

    int cpu_mark[MAX_NCPUS] = {0}, online_ncpus = 0;

    int i, j, n, cpu;
    for (i = 0, cpuinfo = cpuinfo_list; i < cpuinfo_ngroups; i ++, cpuinfo ++) {
        flexsc_assert(cpuinfo->size > 0 && cpuinfo->size <= MAX_CPUINFO_SIZE);
        for (j = 0; j < cpuinfo->size; j ++) {
            cpu = cpuinfo->list[j];
            if (++ cpu_mark[cpu] > 1) {
                flexsc_panic("invalid cpu_mark cpu %d\n", cpu);
            }
        }
        if (cpuinfo->size == 1) {
            online_ncpus += 1;
        }
        else {
            online_ncpus += cpuinfo->size - 1;
        }
    }

    static struct thread_struct __thread_array[MAX_NCPUS];

    for (i = 0; i < MAX_NCPUS; i ++) {
        __init_thread_struct(__thread_array + i);
    }

    for (i = 0, n = 0, cpuinfo = cpuinfo_list; i < cpuinfo_ngroups; i ++, cpuinfo ++) {
        for (j = (cpuinfo->size == 1) ? 0 : 1; j < cpuinfo->size; j ++, n ++) {
            struct thread_struct *this = __thread_array + n;
            this->thread_array = __thread_array;
            this->id = this->fork = n;
            this->cpu = cpuinfo->list[j];
            this->online_ncpus = online_ncpus;
        }
    }
    if (n != online_ncpus) {
        flexsc_panic("n = %d, online_ncpus = %d, failed.", n, online_ncpus);
    }

#ifdef FLEXSC_DEBUG
    flexsc_debug_nolock("online_ncpus = %d\n", online_ncpus);
    for (i = 0, cpuinfo = cpuinfo_list; i < cpuinfo_ngroups; i ++, cpuinfo ++) {
        if (cpuinfo->size != 1) {
            flexsc_debug_nolock("cpu %2d server: ", cpuinfo->list[0]);
            for (j = 1; j < cpuinfo->size; j ++) {
                cpu = cpuinfo->list[j];
                flexsc_debug_nolock(" %2d", cpu);
            }
            flexsc_debug_nolock("\n");
        }
    }
    for (i = 0; i < online_ncpus; i ++) {
        struct thread_struct *thread = __thread_array + i;
        flexsc_debug_nolock("cpu %2d worker: [%d]\n", thread->cpu, thread->id);
    }
#endif

    for (i = 0, cpuinfo = cpuinfo_list; i < cpuinfo_ngroups; i ++, cpuinfo ++) {
        if (cpuinfo->size > 1) {
            __bind_to_cpu(cpuinfo->list[0]);
            syscall3(__NR_flexsc_serve, cpuinfo->list[0], &(cpuinfo->list[1]), cpuinfo->size - 1);
        }
    }

    for (i = 0; i < online_ncpus; i ++) {
        struct thread_struct *this = &__thread_array[i];
        (i == 0 ? __boot_main_cpu : __boot_other_cpu)(this);
        while (!this->boot) {
            cpu_relax();
        }
    }

    start_kernel();
}

static __always_inline void
thread_update_current(struct thread_struct *this, struct fiber_struct *fiber) {
    flexsc_assert(this == __this && this->current != fiber);
    flexsc_assert(fiber_get_state(fiber) == FIBER_STATE_RUNNABLE ||
                  fiber_get_state(fiber) == FIBER_STATE_MONITOR);
    
    thread_set_current(this, fiber);
    
    if (likely((fiber = fiber->switch_from) == NULL)) {
        return ;
    }
    flexsc_assert(fiber != this->syscall && fiber->type != FIBER_TYPE_SYSCALL &&
                  fiber_get_state(fiber) != FIBER_STATE_MONITOR);
    
    switch (fiber_get_state(fiber)) {
    case FIBER_STATE_EXIT:
        flexsc_assert(fiber->type != FIBER_TYPE_SYSCALL);
        if (fiber->detached) {
            fiber_set_state(fiber, FIBER_STATE_UNUSED);
            reclaim_fiber(fiber);
        }
        else {
            fiber_set_state(fiber, FIBER_STATE_ZOMBIE);
        }
        return ;
    default:
        flexsc_panic("invalid fiber state [%d:%d] %d.\n",
                     fiber->fid.gid, fiber->fid.rid, fiber_get_state(fiber));
    }
}

void
fiber_start(struct fiber_struct *fiber, void *(*__fn)(void *arg), void *arg) {
    thread_update_current(__this, fiber);
    
    fiber_exit(fiber, __fn(arg));
    
    flexsc_panic("no return here!!.\n");
}

static __always_inline void
switch_to(struct thread_struct *this,
          struct fiber_struct *prev,
          struct fiber_struct *next,
          struct fiber_struct *from) {
    flexsc_assert(this == __this);
    flexsc_assert(fiber_get_state(prev) != FIBER_STATE_RUNNABLE);
    flexsc_assert(fiber_get_state(next) == FIBER_STATE_RUNNABLE ||
                  fiber_get_state(next) == FIBER_STATE_MONITOR);
    
    next->switch_from = from;

    extern void __switch_to(struct fiber_context *prev, struct fiber_context *next);

    __switch_to(&(prev->context), &(next->context));
    
    thread_update_current(this, prev);
}

void
schedule_fork(struct thread_struct *this, struct fiber_struct *fiber, struct flexsc_sysentry *entry) {
    flexsc_assert(fiber_get_state(fiber) == FIBER_STATE_UNINIT);
    
    entry->owner = fiber;
    fiber_set_state(fiber, FIBER_STATE_SYSCALL);

    if (unlikely(this != NULL)) {
        flexsc_assert(this == __this);
        this->done_pool[this->done_size ++] = entry;
        return ;
    }

    this = __this;
    if (this->fork == this->online_ncpus) {
        this->fork = 0;
    }

    struct thread_struct *thread = this->thread_array + this->fork ++;

    spin_lock(&(thread->fork_lock)); {
        entry->next = thread->fork_head, thread->fork_head = entry;
        thread->fork_flag = 1;
    }
    spin_unlock(&(thread->fork_lock));
}

long
flexsc_nptl_syscall(struct flexsc_sysentry *entry, unsigned int sysnum) {
    struct thread_struct *this = __this;
    struct fiber_struct *prev = this->current, *next;
    struct mbox_struct *sendbox;
    int done_size;

    flexsc_assert(this != NULL && this->current != NULL);
    flexsc_assert(entry->sysnum == sysnum);

    if (unlikely(fiber_get_state(prev) != FIBER_STATE_RUNNABLE)) {
        return syscall6(sysnum,
                        entry->sysargs[0], entry->sysargs[1], entry->sysargs[2],
                        entry->sysargs[3], entry->sysargs[4], entry->sysargs[5]);
    }

    entry->owner = prev;
    fiber_set_state(prev, FIBER_STATE_SYSCALL);

    if (likely((sendbox = this->sendbox) != NULL)) {
        if (likely(-- this->mbox_rest >= 0)) {
            int send = sendbox->send;
            sendbox->pool[send ++] = entry;
            if (unlikely(send == MAX_NFIBERS)) {
                send = 0;
            }
            sendbox->send = send;
            goto syscall_next;
        }
    }
    this->wait_pool[this->wait_size ++] = entry;

syscall_next:
    if (likely((done_size = this->done_size) != 0)) {
        next = this->done_pool[-- done_size]->owner, this->done_size = done_size;
        flexsc_assert(fiber_struct_check(next) == 0 && fiber_get_state(next) == FIBER_STATE_SYSCALL);
        fiber_set_state(next, FIBER_STATE_RUNNABLE);
    }
    else {
        next = this->syscall;
        flexsc_assert(next != NULL && next->type == FIBER_TYPE_SYSCALL &&
                      fiber_get_state(next) == FIBER_STATE_MONITOR);
    }
    switch_to(this, prev, next, NULL);

    flexsc_assert(this == __this && this->current == prev &&
                  fiber_get_state(prev) == FIBER_STATE_RUNNABLE);
    
    return entry->sysret;
}

static void
__syscall_dump(struct thread_struct *this) {
    if (this->wait_size == 0) {
        return ;
    }
    flexsc_debug_lock(); {
        int i;
        for (i = 0; i != this->wait_size; i ++) {
            struct flexsc_sysentry *entry = this->wait_pool[i];
            struct fiber_struct *fiber = entry->owner;
            flexsc_debug_nolock("  %4d[%5d %016lx]: %3d "
                                "%016lx %016lx %016lx %016lx %016lx %016lx\n",
                                i, fiber->fid.gid, (unsigned long)fiber, entry->sysnum,
                                entry->sysargs[0], entry->sysargs[1], entry->sysargs[2],
                                entry->sysargs[3], entry->sysargs[4], entry->sysargs[5]);
        }
    }
    flexsc_debug_unlock();
}

static void
syscall_do_syscall(struct thread_struct *this) {
    flexsc_assert(this == __this);
    
    (void)__syscall_dump;
    /*
    if (unlikely(flexsc_debug_enabled())) {
        __syscall_dump(this);
    }
    */

    flexsc_assert(this->wait_size >= 0 && this->wait_size < MAX_NFIBERS && this->done_size == 0);
    flexsc_assert(this->sendbox == NULL || (this->sendbox->send >= 0 && this->sendbox->send < MAX_NFIBERS));

    int ret;
    if (unlikely((ret = syscall1(__NR_flexsc_syscall, this->wait_size)) < 0)) {
        flexsc_panic("flexsc syscall failed %d(%d):%d.\n", this->id, this->cpu, ret);
    }
    flexsc_assert(this == __this);

    flexsc_assert(ret > 0 && ret < MAX_NFIBERS);

    this->wait_size = 0, this->done_size = ret, this->syscall_counts += ret;

    struct mbox_struct *sendbox;
    if (likely((sendbox = this->sendbox) != NULL)) {
        int send = sendbox->send, recv = sendbox->recv;
        if (unlikely(send < recv)) {
            send += MAX_NFIBERS;
        }
        send -= recv;
        this->mbox_rest = (long)(send + ret) * this->mbox_rate / MBOX_RATE_MAX - send;
    }

    void *pool = this->done_pool;
    this->done_pool = this->_____pool, this->_____pool = pool;
}

static long
__this_set_mbox_rate(struct thread_struct *this, short rate, int test) {
    if (rate >= 0 && rate <= MBOX_RATE_MAX) {
        this->mbox_rate = rate;
        usleep(20000);
        if (test) {
            this->syscall_counts = 0;
            usleep(20000);
            return this->syscall_counts;
        }
    }
    return -1;
}

static void
syscall_idle_main(void) {
    struct thread_struct *this = __this;
    
    flexsc_assert(this != NULL && this->current != NULL);
    (void)this;

    if (this->sendbox == NULL) {
        int skip = 0;
        while (1) {
            (void)skip;
#ifdef FLEXSC_DEBUG
            this->syscall_counts = 0;
            usleep(50000);

            flexsc_assert(this->syscall_counts > 0);
            if (this->syscall_counts <= 1) {
                if (skip) {
                    usleep(1000000);
                    continue ;
                }
                skip = 1;
            }
            else {
                skip = 0;
            }
            flexsc_debug_nolock("%2d %8ld\n", this->cpu, this->syscall_counts);
#endif
            usleep(1000000);
        }
    }
    else if (__idle_main_mbox_rate >= 0 && __idle_main_mbox_rate <= MBOX_RATE_MAX) {
        flexsc_debug_nolock("idle_main %2d choose %d\n", this->cpu, __idle_main_mbox_rate);
        __this_set_mbox_rate(this, __idle_main_mbox_rate, 0);

        int skip = 0;
        while (1) {
#ifdef FLEXSC_DEBUG
            this->syscall_counts = 0;
            usleep(50000);

            flexsc_assert(this->syscall_counts > 0);
            if (this->syscall_counts <= 1) {
                if (skip) {
                    usleep(1000000);
                    continue ;
                }
                skip = 1;
            }
            else {
                skip = 0;
            }
            flexsc_debug_nolock("%2d %8ld\n", this->cpu, this->syscall_counts);
#endif
            usleep(1000000);
        }
    }
    else {
        long next = this->cpu, tick, debug = 0;
        flexsc_debug_nolock("idle_main %2d dynamic\n", this->cpu);
        __this_set_mbox_rate(this, MBOX_RATE_MAX / 2, 0);
        while (1) {
            const int max_steps = 2, delta[] = {0, -1, 1}, n = sizeof(delta) / sizeof(delta[0]);
            long syscall_counts[n];
            short current_rate = this->mbox_rate, max_rate = -1;
            int i, j, max;
            for (i = 0; i < max_steps; i ++) {
                for (j = 0; j < n; j ++) {
                    syscall_counts[j] = __this_set_mbox_rate(this, current_rate + delta[j], 1);
                }
                for (j = 1, max = 0; j < n; j ++) {
                    if (syscall_counts[j] > syscall_counts[max]) {
                        max = j;
                    }
                }
                if (max_rate == -1) {
                    max_rate = current_rate + delta[max];
                }
                else if (max_rate != current_rate + delta[max]) {
                    max_rate = -1;
                    break ;
                }
            }
            if (max_rate != -1) {
                __this_set_mbox_rate(this, max_rate, 0);
            }
            else {
                __this_set_mbox_rate(this, current_rate, 0);
            }

            next = (next * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
            tick = (next >> 12) % 16;
            if (max_rate == -1 || max_rate == current_rate) {
                usleep(100000 + tick * 10000);
            }

            (void)debug;
#ifdef FLEXSC_DEBUG
            if (debug ++ % 10 == 0) {
                flexsc_debug_nolock("%2d mbox_rate = %d\n", this->cpu, this->mbox_rate);
            }
#endif
        }
    }
}

static void
syscall_idle_init(struct thread_struct *this) {
    int ret;
    if ((ret = fiber_create(this, NULL, 0, (void *)syscall_idle_main, NULL)) != 0) {
        flexsc_panic("init: cannot create syscall_idle_main[%d(%d)].\n", this->id, this->cpu);
    }
}

static __always_inline struct fiber_struct *
sched_pick_next(struct thread_struct *this) {
    flexsc_assert(this == __this);
    struct fiber_struct *next;
    int done_size;
    if (likely((done_size = this->done_size) != 0)) {
        next = this->done_pool[-- done_size]->owner, this->done_size = done_size;
        flexsc_assert(fiber_struct_check(next) == 0 && fiber_get_state(next) == FIBER_STATE_SYSCALL);
        fiber_set_state(next, FIBER_STATE_RUNNABLE);
    }
    else {
        next = this->syscall;
        flexsc_assert(next != NULL && next->type == FIBER_TYPE_SYSCALL &&
                      fiber_get_state(next) == FIBER_STATE_MONITOR);
    }
    return next;
}

void
schedule_exit(struct thread_struct *this, struct fiber_struct *fiber) {
    flexsc_assert(this == __this && this->current == fiber &&
                  fiber_get_state(fiber) == FIBER_STATE_EXIT);

    struct fiber_struct *next = sched_pick_next(this);
    switch_to(this, fiber, next, fiber);

    flexsc_panic("no return here!!.\n");
}

void __noreturn
syscall_main(void) {
    struct thread_struct *this = __this;
    
    flexsc_assert(this != NULL && this->current != NULL);
    
    struct fiber_struct *prev = this->current, *next;
    
    flexsc_assert(prev->type == FIBER_TYPE_SYSCALL &&
                  fiber_get_state(prev) == FIBER_STATE_MONITOR);
    
    syscall_idle_init(this);

    while (1) {
        int done_size;
        while ((done_size = this->done_size) != 0) {
            next = this->done_pool[-- done_size]->owner, this->done_size = done_size;
            flexsc_assert(fiber_struct_check(next) == 0 && fiber_get_state(next) == FIBER_STATE_SYSCALL);
            fiber_set_state(next, FIBER_STATE_RUNNABLE);
            switch_to(this, prev, next, NULL);
        }

        flexsc_assert(this == __this && this->current == prev &&
                      fiber_get_state(prev) == FIBER_STATE_MONITOR);

        syscall_do_syscall(this);

        if (unlikely(this->fork_flag)) {
            struct flexsc_sysentry *head;
            spin_lock(&(this->fork_lock)); {
                head = this->fork_head, this->fork_head = NULL;
                this->fork_flag = 0;
            }
            spin_unlock(&(this->fork_lock));

            while (head != NULL) {
                this->done_pool[this->done_size ++] = head, head = head->next;
            }
        }
    }
}
