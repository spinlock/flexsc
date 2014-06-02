#include "flexsc.h"

static int
__thread_main(void *arg) {
    struct task_struct *task;
    struct flexsc_kstruct *kstruct;
    struct flexsc_syspage *syspage;
    int cpu;

    task = current;
    if ((kstruct = task->kstruct) == NULL) {
        return -ERR_KTHREAD_KSTRUCT;
    }
    if (task->syspage != NULL) {
        return -ERR_KTHREAD_SYSPAGE;
    }

    cpu = (long)arg;
    if (check_cpumask(task->pid, cpu) != 0) {
        return -ERR_KTHREAD_CPUMASK;
    }
    flexsc_assert(smp_processor_id() == cpu);

    syspage = kstruct->syspages + cpu;

    task_set_syspage(task, syspage);
    switch (syspage->__type) {
    case TYPE_SERVER:
        return __flexsc_kserver_main(task);
    case TYPE_WORKER:
        return __flexsc_kworker_main(task);
    default:
        return __flexsc_kdefault_main(task);
    }
}

int
create_worker(struct flexsc_syspage *syspage, int cpu) {
    const unsigned long flags = CLONE_VM | CLONE_FS | CLONE_FILES;
    void *arg = (void *)(long)cpu;
    int i, ret;

    flexsc_assert(smp_processor_id() == cpu);

    if (likely(!syspage->fork_flag)) {
        syspage->fork_flag = 1, syspage->worker += MAX_KTHREADS_GROUP;
        for (i = 0; i < MAX_KTHREADS_GROUP; i ++) {
            if ((ret = kernel_thread(__thread_main, arg, flags)) < 0) {
                syspage->worker -= (MAX_KTHREADS_GROUP - i);
                return ret;
            }
        }
        syspage->fork_flag = 0;
    }
    return 0;
}
