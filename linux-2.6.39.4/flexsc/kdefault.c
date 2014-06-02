#include "flexsc.h"

static void
default_get_schedule(struct task_struct *task) {
    struct flexsc_syspage *syspage = task->syspage;
    struct task_struct *worker, *waiter;
    int err, cpu;
    
    flexsc_assert(smp_processor_id() == (int)(syspage - task->kstruct->syspages));
    flexsc_assert(syspage->worker > 0);

    if (likely(-- syspage->worker != 0)) {
        return ;
    }

    if (likely(syspage->wait_size != 0)) {
        goto wakeup_worker;
    }

    if (unlikely(syspage->done_size == 0)) {
        return ;
    }

    if (unlikely((waiter = syspage->waiter) == NULL)) {
        return ;
    }

    flexsc_assert(waiter != NULL && waiter == syspage->waiter);
    syspage->waiter = NULL;
    wakeup_task(waiter);
    return ;

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
}

static void
default_put_schedule(struct task_struct *task) {
    struct flexsc_syspage *syspage = task->syspage;

    flexsc_assert(smp_processor_id() == (int)(syspage - task->kstruct->syspages));

    syspage->worker ++;
}

static void
default_work(int cpu, struct task_struct *task,
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
default_wait(int cpu, struct task_struct *task,
             struct flexsc_syspage *syspage) {
    struct task_struct *waiter;
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

    if (unlikely(syspage->done_size == 0)) {
        goto worker_sleep;
    }

    if (unlikely((waiter = syspage->waiter) == NULL)) {
        goto worker_sleep;
    }

    set_current_state(TASK_UNINTERRUPTIBLE);
    list_add(&(task->worker_link), &(syspage->worker_list));

    flexsc_assert(waiter != NULL && waiter == syspage->waiter);
    syspage->waiter = NULL;
    wakeup_task(waiter);

    schedule();
    return 0;

worker_sleep:
    set_current_state(TASK_UNINTERRUPTIBLE);
    list_add(&(task->worker_link), &(syspage->worker_list));
    schedule();
    return 0;
}

int
__flexsc_kdefault_main(struct task_struct *task) {
    struct flexsc_kstruct *kstruct = task->kstruct;
    struct flexsc_syspage *syspage = task->syspage;
    int err, cpu;
    
    flexsc_assert(kstruct != NULL && syspage != NULL);
    
    task->flexsc_get_schedule = default_get_schedule;
    task->flexsc_put_schedule = default_put_schedule;

    cpu = syspage - kstruct->syspages;
    while ((err = default_wait(cpu, task, syspage)) == 0) {
        default_work(cpu, task, syspage);
    }
    
    if (err != -1) {
        printk("default exit : %6d,%d\n", task->pid, -err);
    }
    return 0;
}
