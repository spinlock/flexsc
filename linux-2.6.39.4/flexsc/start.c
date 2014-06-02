#include "flexsc.h"

asmlinkage void
sys_flexsc_start(int cpu, struct sysbox_pool *__pool) {
    struct task_struct *task;
    struct flexsc_kstruct *kstruct;
    struct flexsc_syspage *syspage;
    struct sysbox_pool s_pool, *pool = memset(&s_pool, 0, sizeof(s_pool));
    int exit_code;

    if (!(cpu >= 0 && cpu < MAX_NCPUS)) {
        exit_code = -ERR_START_CPU;
        goto err_exit;
    }

    task = current;
    if ((kstruct = task->kstruct) == NULL) {
        exit_code = -ERR_START_KSTRUCT;
        goto err_exit;
    }
    if ((syspage = task->syspage) != NULL) {
        exit_code = -ERR_START_SYSPAGE;
        goto err_exit;
    }

    if ((exit_code = check_cpumask(task->pid, cpu)) != 0) {
        goto err_exit;
    }
    flexsc_assert(smp_processor_id() == cpu);

    if (kstruct->exit) {
        exit_code = -ERR_START_SYSPAGE_STATE;
        goto err_exit;
    }
    
    syspage = kstruct->syspages + cpu;

    spin_lock(&(kstruct->lock));
    if (kstruct->exit || syspage->__stat != STAT_NONE) {
        spin_unlock(&(kstruct->lock));
        exit_code = -ERR_START_SYSPAGE_STATE;
        goto err_exit;
    }
    syspage->__stat = STAT_WORK, syspage->worker = 0;
    spin_unlock(&(kstruct->lock));

    task_set_syspage(task, syspage);

    pool->wait_pool = syspage->wait_pool;
    pool->_____pool = syspage->_____pool;
    pool->done_pool = syspage->done_pool;
    if (syspage->__type == TYPE_WORKER) {
        pool->sendbox = syspage->mbox_array[0].__sendbox;
        pool->recvbox = syspage->mbox_array[0].__recvbox;
    }

    if (copy_to_user(__pool, pool, sizeof(struct sysbox_pool)) != 0) {
        exit_code = -ERR_START_SYSBOX_POOL;
        goto err_exit;
    }

    if ((exit_code = create_worker(syspage, cpu)) != 0) {
        goto err_exit;
    }

    printk("linux %3d %016lx %016lx %016lx %016lx %016lx %016lx\n",
           cpu, sizeof(struct flexsc_sysentry *) * MAX_NFIBERS,
           (long)(syspage->wait_pool), (long)(syspage->_____pool),
           (long)(syspage->done_pool), (long)(pool->sendbox), (long)(pool->recvbox));

    kstruct->start = 1;

#ifdef FLEXSC_DEBUG
    syspage->mon_show = 1;
#endif

    flexsc_start_hook();
    printk("start kernel on cpu %2d %d\n", cpu, task->pid);
    return ;

err_exit:
    printk("exit flexsc_start with %d [%d]\n", smp_processor_id(), exit_code);
    sys_exit_group(exit_code);
}
