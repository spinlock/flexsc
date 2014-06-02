#include "flexsc.h"

static __always_inline int
try_pullmsg(struct mbox_struct *recvbox, struct flexsc_syspage *syspage) {
    int recv = recvbox->recv, send = recvbox->send;
    int size, entries;
    void *dst, *src;
    if (unlikely(recv == send)) {
        return 0;
    }
    if (likely(recv < send)) {
        entries = send - recv, size = sizeof(struct flexsc_sysentry *) * entries;
        dst = syspage->done_pool + syspage->done_size, src = recvbox->pool + recv;
        memcpy(dst, src, size);
        syspage->done_size += entries, recvbox->recv = send;
    }
    else {
        entries = MAX_NFIBERS - recv, size = sizeof(struct flexsc_sysentry *) * entries;
        dst = syspage->done_pool + syspage->done_size, src = recvbox->pool + recv;
        memcpy(dst, src, size);
        if (likely(send != 0)) {
            entries += send;
            memcpy(dst + size, recvbox->pool, sizeof(struct flexsc_sysentry *) * send);
        }
        syspage->done_size += entries, recvbox->recv = send;
    }
    return entries;
}

static void
worker_get_schedule(struct task_struct *task) {
    struct flexsc_syspage *syspage = task->syspage;
    struct task_struct *worker, *waiter;
    struct mbox_struct *sendbox;
    struct mbox_struct *recvbox;
    int err, cpu;
    
    flexsc_assert(smp_processor_id() == (int)(syspage - task->kstruct->syspages));
    flexsc_assert(syspage->worker > 0);

    if (likely(-- syspage->worker != 0)) {
        return ;
    }

    if (likely(syspage->wait_size != 0)) {
        goto wakeup_worker;
    }

    if (unlikely((waiter = syspage->waiter) == NULL)) {
        return ;
    }

    recvbox = syspage->mbox_array[0].__recvbox;
    if (likely(try_pullmsg(recvbox, syspage) != 0 || syspage->done_size != 0)) {
        goto wakeup_waiter;
    }

    sendbox = syspage->mbox_array[0].__sendbox;
    if (unlikely(sendbox->send == recvbox->recv)) {
        return ;
    }

wakeup_worker:
    if (likely((worker = alloc_worker(syspage)) != NULL)) {
        wakeup_task(worker);
    }
    else {
        cpu = syspage - task->kstruct->syspages;
        if ((err = create_worker(syspage, cpu)) != 0) {
            drop_flexsc_kstruct(task->kstruct, err);
        }
    }
    return ;

wakeup_waiter:
    flexsc_assert(syspage->wait_size == 0 && syspage->done_size != 0);
    flexsc_assert(waiter != NULL && waiter == syspage->waiter);
    syspage->waiter = NULL;
    wakeup_task(waiter);
    return ;
}

static void
worker_put_schedule(struct task_struct *task) {
    struct flexsc_syspage *syspage = task->syspage;

    flexsc_assert(smp_processor_id() == (int)(syspage - task->kstruct->syspages));

    syspage->worker ++;
}

static void
worker_work(int cpu, struct task_struct *task,
            struct flexsc_syspage *syspage) {
    struct flexsc_sysentry *syscall;
    
    flexsc_assert(smp_processor_id() == cpu);
    flexsc_assert(list_empty(&(task->worker_link)));

    while (!syspage_mayexit(syspage) && syspage->wait_size != 0) {
        syscall = syspage->wait_pool[-- syspage->wait_size];

#ifdef FLEXSC_DEBUG
        if (likely(syscall->sysname < __NR_syscall_max)) {
            syspage->mon_syscall[syscall->sysname] ++;
        }
#endif

        set_worker_kstate(task);

        syscall->sysret = do_syscall(syscall->sysname, syscall->sysargs);

        flexsc_assert(smp_processor_id() == cpu);
        clear_worker_kstate(task);
        syspage->done_pool[syspage->done_size ++] = syscall;
    }
}

static int
worker_wait(int cpu, struct task_struct *task,
            struct flexsc_syspage *syspage) {
    struct task_struct *waiter;
    struct mbox_struct *sendbox;
    struct mbox_struct *recvbox;

#ifdef FLEXSC_DEBUG
    int try_again = 0;
#endif

top:

#ifdef FLEXSC_DEBUG
    if (unlikely(syspage->mon_wait_reset)) {
        syspage->mon_wait_step0 = 0;
        syspage->mon_wait_step1 = 0;
        syspage->mon_wait_reset = 0;
        try_again = 0;
    }
#endif

    flexsc_assert(smp_processor_id() == cpu);
    flexsc_assert(syspage->worker > 0);

    if (unlikely(syspage_mayexit(syspage))) {
        return -1;
    }
    
    if (unlikely(syspage->wait_size != 0)) {
        return 0;
    }

    if (likely(-- syspage->worker != 0)) {
        goto worker_sleep;
    }

    if (unlikely((waiter = syspage->waiter) == NULL)) {
        goto worker_sleep;
    }

    recvbox = syspage->mbox_array[0].__recvbox;
    if (likely(try_pullmsg(recvbox, syspage) != 0 || syspage->done_size != 0)) {
        goto wakeup_waiter;
    }

    sendbox = syspage->mbox_array[0].__sendbox;
    if (likely(sendbox->send != recvbox->recv)) {
        goto worker_retry;
    }

worker_sleep:
    set_current_state(TASK_UNINTERRUPTIBLE);
    list_add(&(task->worker_link), &(syspage->worker_list));
    schedule();
    return 0;

wakeup_waiter:
    flexsc_assert(syspage->wait_size == 0 && syspage->done_size != 0);
    flexsc_assert(waiter != NULL && waiter == syspage->waiter);
    set_current_state(TASK_UNINTERRUPTIBLE);
    list_add(&(task->worker_link), &(syspage->worker_list));
    syspage->waiter = NULL;
    wakeup_task(waiter);
    schedule();
    return 0;

worker_retry:
#ifdef FLEXSC_DEBUG
    syspage->mon_wait_step0 ++;
    if (!try_again) {
        syspage->mon_wait_step1 ++;
    }
    try_again = 1;
#endif

    syspage->worker ++;
    yield();
    goto top;
}

int
__flexsc_kworker_main(struct task_struct *task) {
    struct flexsc_kstruct *kstruct = task->kstruct;
    struct flexsc_syspage *syspage = task->syspage;
    int err, cpu;
    
    flexsc_assert(kstruct != NULL && syspage != NULL);
    
    task->flexsc_get_schedule = worker_get_schedule;
    task->flexsc_put_schedule = worker_put_schedule;

    flexsc_assert(syspage->mbox_used == 1);

    cpu = syspage - kstruct->syspages;
    while ((err = worker_wait(cpu, task, syspage)) == 0) {
        worker_work(cpu, task, syspage);
    }
    
    if (err != -1) {
        printk("worker exit : %6d,%d\n", task->pid, -err);
    }
    return 0;
}
