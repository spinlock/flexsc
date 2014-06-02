#include "flexsc.h"

static void
mbox_bind(struct flexsc_syspage *server, struct flexsc_syspage *worker) {
    struct mbox_struct *mbox0, *mbox1;
    mbox0 = server->__mbox_pool + server->mbox_used;
    mbox1 = worker->__mbox_pool + worker->mbox_used;
    server->mbox_array[server->mbox_used].__sendbox = mbox0;
    server->mbox_array[server->mbox_used].__recvbox = mbox1;
    worker->mbox_array[worker->mbox_used].__sendbox = mbox1;
    worker->mbox_array[worker->mbox_used].__recvbox = mbox0;
    server->mbox_used ++, worker->mbox_used ++;
}

asmlinkage void
sys_flexsc_serve(int cpu, int *__cpulist, size_t size) {
    struct task_struct *task;
    struct flexsc_kstruct *kstruct;
    struct flexsc_syspage *syspage, *syspage_worker;
    int cpulist[MAX_NCPUS];
    int i, exit_code;

    if (!(cpu >= 0 && cpu < MAX_NCPUS)) {
        exit_code = -ERR_SERVE_CPU;
        goto err_exit;
    }

    task = current;
    if ((kstruct = task->kstruct) == NULL) {
        exit_code = -ERR_SERVE_KSTRUCT;
        goto err_exit;
    }
    if ((syspage = task->syspage) != NULL) {
        exit_code = -ERR_SERVE_SYSPAGE;
        goto err_exit;
    }

    if (!(size > 0 && size <= MAX_NCPUS)) {
        exit_code = -ERR_SERVE_SIZE;
        goto err_exit;
    }

    if (copy_from_user(cpulist, __cpulist, sizeof(cpulist[0]) * size) != 0) {
        exit_code = -ERR_SERVE_CPULIST;
        goto err_exit;
    }

    for (i = 0; i < size; i ++) {
        if (cpulist[i] >= 0 && cpulist[i] < MAX_NCPUS) {
            continue ;
        }
        exit_code = -ERR_SERVE_WORKER_CPU;
        goto err_exit;
    }

    if (kstruct->start || kstruct->exit) {
        exit_code = -ERR_SERVE_SYSPAGE_STATE;
        goto err_exit;
    }

    syspage = kstruct->syspages + cpu;
    spin_lock(&(kstruct->lock));
    if (kstruct->start || kstruct->exit || syspage->__stat != STAT_NONE || syspage->__type != TYPE_DEFAULT) {
        spin_unlock(&(kstruct->lock));
        exit_code = -ERR_SERVE_SYSPAGE_STATE;
        goto err_exit;
    }
    syspage->__stat = STAT_WORK, syspage->worker = 0, syspage->__type = TYPE_SERVER;
    for (i = 0; i < size; i ++) {
        syspage_worker = kstruct->syspages + cpulist[i];
        if (syspage_worker->__stat == STAT_NONE && syspage_worker->__type == TYPE_DEFAULT) {
            syspage_worker->__type = TYPE_WORKER;
            mbox_bind(syspage, syspage_worker);
            continue ;
        }
        spin_unlock(&(kstruct->lock));
        exit_code = -ERR_SERVE_SYSPAGE_WORKER_STATE;
        goto err_exit;
    }
    spin_unlock(&(kstruct->lock));

    if ((exit_code = create_worker(syspage, cpu)) != 0) {
        goto err_exit;
    }

#ifdef FLEXSC_DEBUG
    syspage->mon_show = 1;
#endif

    printk("start server on cpu %3d\n", cpu);
    return;

err_exit:
    printk("exit flexsc_serve with %d [%d]\n", smp_processor_id(), exit_code);
    sys_exit_group(exit_code);
}
