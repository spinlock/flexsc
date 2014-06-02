#include "flexsc.h"

static void
server_get_schedule(struct task_struct *task) {
    struct flexsc_syspage *syspage = task->syspage;
    struct task_struct *worker;
    int err, cpu;
    
    flexsc_assert(smp_processor_id() == (int)(syspage - task->kstruct->syspages));
    flexsc_assert(syspage->worker > 0);

    if (likely(-- syspage->worker != 0)) {
        return ;
    }

    if (likely((worker = alloc_worker(syspage)) != NULL)) {
        wakeup_task(worker);
    }
    else {
        cpu = syspage - task->kstruct->syspages;
        if ((err = create_worker(syspage, cpu)) != 0) {
            drop_flexsc_kstruct(task->kstruct, err);
        }
    }
}

static void
server_put_schedule(struct task_struct *task) {
    struct flexsc_syspage *syspage = task->syspage;

    flexsc_assert(smp_processor_id() == (int)(syspage - task->kstruct->syspages));

    syspage->worker ++;
}

static void
server_work(int cpu, struct task_struct *task,
            struct flexsc_syspage *syspage) {
    struct flexsc_sysentry *syscall;
    struct mbox_struct *sendbox, *recvbox;
    int mbox_used, mbox, recv, send, miss;

    flexsc_assert(smp_processor_id() == cpu);
    flexsc_assert(list_empty(&(task->worker_link)));

    mbox_used = syspage->mbox_used, miss = 0;
    while (!syspage_mayexit(syspage)) {
        mbox = syspage->mbox_mbox;

        recvbox = syspage->mbox_array[mbox].__recvbox;

        recv = recvbox->recv, send = recvbox->send;
        if (unlikely(recv == send)) {
            if (unlikely((++ miss) == mbox_used * 4)) {
                return ;
            }
            if (unlikely((++ mbox) == mbox_used)) {
                mbox = 0;
            }
            syspage->mbox_mbox = mbox;
            continue ;
        }
        miss = 0;

        syscall = (struct flexsc_sysentry *)recvbox->pool[recv ++];
        if (unlikely(recv == MAX_NFIBERS)) {
            recv = 0;
        }
        recvbox->recv = recv;

        sendbox = syspage->mbox_array[mbox].__sendbox;

        if (unlikely((recv & 7) == 0)) {
            if ((++ mbox) == mbox_used) {
                mbox = 0;
            }
            syspage->mbox_mbox = mbox;
        }

#ifdef FLEXSC_DEBUG
        if (likely(syscall->sysname < __NR_syscall_max)) {
            syspage->mon_syscall[syscall->sysname] ++;
        }
#endif

        set_worker_kstate(task);

        syscall->sysret = do_syscall(syscall->sysname, syscall->sysargs);

        flexsc_assert(smp_processor_id() == cpu);
        clear_worker_kstate(task);

        send = sendbox->send;
        sendbox->pool[send ++] = syscall;
        if (unlikely(send == MAX_NFIBERS)) {
            send = 0;
        }
        sendbox->send = send;
    }
}

static int
server_wait(int cpu, struct task_struct *task,
            struct flexsc_syspage *syspage) {
    struct mbox_struct *recvbox;
    int i, mbox_used;

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

    mbox_used = syspage->mbox_used;
    for (i = 0; i < mbox_used; i ++) {
        recvbox = syspage->mbox_array[i].__recvbox;
        if (unlikely(recvbox->recv != recvbox->send)) {
            return 0;
        }
    }

    if (unlikely(-- syspage->worker == 0)) {
        goto worker_retry;
    }

    set_current_state(TASK_UNINTERRUPTIBLE);
    list_add(&(task->worker_link), &(syspage->worker_list));
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
__flexsc_kserver_main(struct task_struct *task) {
    struct flexsc_kstruct *kstruct = task->kstruct;
    struct flexsc_syspage *syspage = task->syspage;
    int err, cpu;
    
    flexsc_assert(kstruct != NULL && syspage != NULL);
    
    task->flexsc_get_schedule = server_get_schedule;
    task->flexsc_put_schedule = server_put_schedule;

    flexsc_assert(syspage->mbox_used >= 1);

    cpu = syspage - kstruct->syspages;
    while ((err = server_wait(cpu, task, syspage)) == 0) {
        server_work(cpu, task, syspage);
    }
    
    if (err != -1) {
        printk("server exit : %6d,%d\n", task->pid, -err);
    }
    return 0;
}
