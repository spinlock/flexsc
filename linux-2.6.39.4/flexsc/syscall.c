#include "flexsc.h"

static int
flexsc_syscall(struct task_struct *task, struct flexsc_syspage *syspage, int wait_size) {
    struct task_struct *worker;
    int ret, cpu = syspage - task->kstruct->syspages;
    void *tmp;

    flexsc_assert(smp_processor_id() == cpu);

    if (unlikely(!(wait_size >= 0 && wait_size < MAX_NFIBERS))) {
        return -ERR_SYSCALL_WAIT_SIZE;
    }
    flexsc_assert(syspage->wait_size == 0);

#ifdef FLEXSC_DEBUG
    syspage->mon_wait_size[0] += wait_size;
    syspage->mon_wait_size[1] += wait_size * wait_size;
    syspage->mon_wait_size[2] ++;
#endif
    
    if (unlikely(wait_size == 0)) {
        goto worker_done;
    }
    
    syspage->wait_size = wait_size;

    if (unlikely(syspage->worker != 0)) {
        goto waiter_sleep;
    }
    
    if (likely((worker = alloc_worker(syspage)) != NULL)) {
        wakeup_task(worker);
    }
    else {
        if ((ret = create_worker(syspage, cpu)) != 0) {
            return ret;
        }
    }

    if (unlikely(syspage->wait_size == 0)) {
        goto worker_done;
    }

waiter_sleep:
    flexsc_assert(syspage->waiter == NULL);
    
    set_current_state(TASK_UNINTERRUPTIBLE);
    syspage->waiter = task;

    schedule();
        
    flexsc_assert(smp_processor_id() == cpu);
    flexsc_assert(syspage->waiter == NULL);

out:
    flexsc_assert(syspage_mayexit(syspage) || (syspage->wait_size == 0 && syspage->done_size != 0));

    tmp = syspage->done_pool, syspage->done_pool = syspage->_____pool, syspage->_____pool = tmp;

    ret = syspage->done_size, syspage->done_size = 0;

#ifdef FLEXSC_DEBUG
    if (unlikely(syspage->mon_reset)) {
        memset(syspage->mon_wait_size, 0, sizeof(syspage->mon_wait_size));
        memset(syspage->mon_done_size, 0, sizeof(syspage->mon_done_size));
        syspage->mon_reset = 0;
    }
    else {
        do {
            unsigned long done_size = ret;
            syspage->mon_done_size[0] += done_size;
            syspage->mon_done_size[1] += done_size * done_size;
            syspage->mon_done_size[2] ++;
        } while (0);
    }
#endif
 
    return ret;

worker_done:
    if (unlikely(syspage->worker != 0)) {
        goto waiter_sleep;
    }

    if (likely(syspage->done_size != 0)) {
        goto out;
    }

    if (unlikely(syspage->__type != TYPE_WORKER)) {
        goto waiter_sleep;
    }

    if (likely((worker = alloc_worker(syspage)) != NULL)) {
        wakeup_task(worker);
    }
    else {
        if ((ret = create_worker(syspage, cpu)) != 0) {
            return ret;
        }
    }
    goto worker_done;
}

asmlinkage int
sys_flexsc_syscall(int wait_size) {
    struct task_struct *task;
    struct flexsc_syspage *syspage;

    int ret, exit_code;

    task = current;
    if (unlikely((syspage = task->syspage) == NULL)) {
        exit_code = -ERR_SYSCALL_SYSPAGE;
        goto err_exit;
    }

    if (unlikely(syspage_mayexit(syspage))) {
        goto syspage_exit;
    }

    if ((ret = flexsc_syscall(task, syspage, wait_size)) < 0) {
        exit_code = ret;
        goto err_exit;
    }

    if (unlikely(syspage_mayexit(syspage))) {
        goto syspage_exit;
    }
    return ret;

err_exit:
    printk("exit flexsc_syscall with %d [%d]\n", smp_processor_id(), exit_code);
    sys_exit_group(exit_code);
    return -1;

syspage_exit:
    if (syspage->__stat == STAT_EXIT) {
        exit_code = task->kstruct->exit_code;
    }
    else {
        exit_code = -ERR_SYSCALL_SYSPAGE_STATE;
    }
    goto err_exit;
}
