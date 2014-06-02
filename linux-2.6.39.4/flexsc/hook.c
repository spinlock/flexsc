#include "flexsc.h"

static volatile long hook_enable = 0;
static volatile long hook_counts = 0;

asmlinkage void
flexsc_syscall_hook(unsigned int sysname, const long sysargs[]) {
    struct task_struct *task;
    int cpu;

    if (likely(!hook_enable)) {
        return ;
    }

    task = current;
    if (likely(task->syspage == NULL)) {
        return ;
    }

    flexsc_assert(sysname < __SYSNUM_flexsc_base);

    cpu = task->syspage - task->kstruct->syspages;
    printk("hook[%4d]:[%5d][%2d] %3d "
           "%016lx %016lx %016lx %016lx %016lx %016lx\n",
           (int)(hook_counts ++), task->pid, cpu, sysname,
           sysargs[0], sysargs[1], sysargs[2],
           sysargs[3], sysargs[4], sysargs[5]);
}

void
flexsc_start_hook(void) {
    if (!xchg(&hook_enable, 1)) {
        printk("flexsc start syscall_hook.\n");
    }
}
