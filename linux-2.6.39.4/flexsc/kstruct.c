#include "flexsc.h"

static void
syspage_init(struct flexsc_syspage *syspage) {
    static_assert(sizeof(struct sysbox_pool) == 40);
    INIT_LIST_HEAD(&(syspage->worker_list));
    syspage->waiter = NULL;

    syspage->worker = 0;
    syspage->__stat = STAT_NONE;
    syspage->__type = TYPE_DEFAULT;
    syspage->fork_flag = 0;

    syspage->wait_size = syspage->done_size = 0;

    syspage->mbox_mbox = 0;
    syspage->mbox_used = 0;

    syspage->wait_pool = syspage->__sysbox_pool + 0 * MAX_NFIBERS;
    syspage->done_pool = syspage->__sysbox_pool + 1 * MAX_NFIBERS;
    syspage->_____pool = syspage->__sysbox_pool + 2 * MAX_NFIBERS;

    memset(syspage->mbox_array, 0, sizeof(syspage->mbox_array));

#ifdef FLEXSC_DEBUG
    syspage->mon_show = 0;
    memset(syspage->mon_syscall, 0, sizeof(syspage->mon_syscall));
    syspage->mon_wait_step0 = 0;
    syspage->mon_wait_step1 = 0;
    memset(syspage->mon_wait_size, 0, sizeof(syspage->mon_wait_size));
    memset(syspage->mon_done_size, 0, sizeof(syspage->mon_done_size));
    syspage->mon_reset = 0;
    syspage->mon_wait_reset = 0;
    memset(syspage->mon_mbox_size, 0, sizeof(syspage->mon_mbox_size));
    syspage->mon_ticks = 0;
#endif
}

struct flexsc_kstruct *
alloc_flexsc_kstruct(struct flexsc_initinfo *info) {
    struct flexsc_kstruct *kstruct;
    int i;
    if ((kstruct = kmalloc(sizeof(struct flexsc_kstruct), GFP_KERNEL)) == NULL) {
        return NULL;
    }

    if ((kstruct->syspages = flexsc_mmap(sizeof(struct flexsc_syspage) * MAX_NCPUS, 1, NULL)) == NULL) {
        kfree(kstruct);
        return NULL;
    }

    for (i = 0; i < MAX_NCPUS; i ++) {
        syspage_init(kstruct->syspages + i);
    }

    spin_lock_init(&(kstruct->lock));
    atomic_set(&(kstruct->count), 1);
    kstruct->start = 0, kstruct->exit = 0, kstruct->exit_code = 0;

    kstruct->stackbase = info->stackbase;
    kstruct->stacksize = info->stacksize;
    return kstruct;
}

struct flexsc_kstruct *
copy_flexsc_kstruct(struct task_struct *task) {
    struct flexsc_kstruct *kstruct;
    if ((kstruct = task->kstruct) != NULL && !(kstruct->exit)) {
        atomic_inc(&(kstruct->count));
        return kstruct;
    }
    return NULL;
}

#ifdef FLEXSC_DEBUG

static void
reclaim_flexsc_kstruct(struct flexsc_kstruct *kstruct) {
    struct flexsc_syspage *syspage;
    unsigned long sum, total;
    int i, j, k, skip;

    flexsc_assert(kstruct->exit && atomic_read(&(kstruct->count)) == 0);

    for (i = 0, syspage = kstruct->syspages; i < MAX_NCPUS; i ++, syspage ++) {
        flexsc_assert(syspage->__stat == STAT_EXIT);
        flexsc_assert(list_empty(&(syspage->worker_list)));
    }

    total = 0;
    for (i = 0, syspage = kstruct->syspages; i < MAX_NCPUS; i ++, syspage ++) {
        for (j = 0; j < __NR_syscall_max; j ++) {
            total += syspage->mon_syscall[j];
        }
    }

    printk("syscall: %ld : ", total);
    for (i = 0, syspage = kstruct->syspages; i < MAX_NCPUS; i ++, syspage ++) {
        if (syspage->mon_show) {
            printk(" %d", i);
        }
    }
    printk("\n");

    for (j = 0; j < __NR_syscall_max; j ++) {
        for (i = 0, skip = 1, syspage = kstruct->syspages; i < MAX_NCPUS && skip; i ++, syspage ++) {
            if (syspage->mon_syscall[j] != 0) {
                skip = 0;
            }
        }
        if (skip) {
            continue ;
        }
        sum = 0;
        for (i = 0, syspage = kstruct->syspages; i < MAX_NCPUS; i ++, syspage ++) {
            sum += syspage->mon_syscall[j];
        }
        printk("   %3d: %2d", j, (int)(sum * 100 / total));
        for (i = 0, k = 0, syspage = kstruct->syspages; i < MAX_NCPUS; i ++, syspage ++) {
            if (!(syspage->mon_show)) {
                continue ;
            }
            if (k != 0 && k % 8 == 0) {
                printk("\n           ");
            }
            k ++;
            printk(" %10ld", syspage->mon_syscall[j]);
        }
        printk("\n");
    }

    printk("bind mbox:\n");
    for (i = 0, syspage = kstruct->syspages; i < MAX_NCPUS; i ++, syspage ++) {
        if (syspage->mbox_used != 0) {
            printk("    %2d: ", i);
            for (j = 0; j < syspage->mbox_used; j ++) {
                if (j != 0) {
                    printk("        ");
                }
                printk("%016lx %016lx\n",
                       (long)syspage->mbox_array[j].__sendbox,
                       (long)syspage->mbox_array[j].__recvbox);
            }
        }
    }

    printk("ticks:\n");
    for (i = 0, syspage = kstruct->syspages; i < MAX_NCPUS; i ++, syspage ++) {
        if (syspage->mon_ticks != 0) {
            printk("  %2d %ld\n", i, syspage->mon_ticks);
        }
    }

    kfree(kstruct);
}

#else

static void
reclaim_flexsc_kstruct(struct flexsc_kstruct *kstruct) {
    struct flexsc_syspage *syspage;
    int i;

    flexsc_assert(kstruct->exit && atomic_read(&(kstruct->count)) == 0);

    syspage = kstruct->syspages;
    for (i = 0; i < MAX_NCPUS; i ++, syspage ++) {
        flexsc_assert(syspage->__stat == STAT_EXIT);
        flexsc_assert(list_empty(&(syspage->worker_list)));
    }
    kfree(kstruct);
}

#endif

static void
drop_flexsc_syspage(int cpu, struct flexsc_syspage *syspage) {
    struct list_head *list, *next, worker_list;
    struct task_struct *waiter;
    
    flexsc_assert(smp_processor_id() == cpu);
    
    syspage->__stat = STAT_EXIT;
    
    INIT_LIST_HEAD(&worker_list);
    
    list = &(syspage->worker_list);
    while ((next = list->next) != list) {
        list_del(next);
        list_add(next, &worker_list), syspage->worker ++;
    }
    waiter = syspage->waiter, syspage->waiter = NULL;

    list = &worker_list;
    while ((next = list->next) != list) {
        list_del_init(next);
        wakeup_task(link_to_task(next));
    }
    if (waiter != NULL) {
        wakeup_task(waiter);
    }
}

void
drop_flexsc_kstruct(struct flexsc_kstruct *kstruct, int exit_code) {
    struct flexsc_syspage *syspage;
    int i;

    if (kstruct->exit) {
        return ;
    }

    spin_lock(&(kstruct->lock));
    if (kstruct->exit) {
        spin_unlock(&(kstruct->lock));
        return ;
    }
    kstruct->exit_code = ((exit_code >> 8) & 0xff), kstruct->exit = 1;
    spin_unlock(&(kstruct->lock));

    syspage = kstruct->syspages;
    for (i = 0; i < MAX_NCPUS; i ++, syspage ++) {
        syspage->__stat = STAT_EXIT;
    }

    printk("drop flexsc_kstruct with %d %d\n", smp_processor_id(), exit_code);
}

void
exit_flexsc_kstruct(struct task_struct *task, int exit_code) {
    struct flexsc_kstruct *kstruct;
    struct flexsc_syspage *syspage;
    
    if ((kstruct = task->kstruct) == NULL) {
        return ;
    }

    drop_flexsc_kstruct(kstruct, exit_code);
    
    if ((syspage = task->syspage) != NULL) {
        flexsc_assert(list_empty(&(task->worker_link)));
        drop_flexsc_syspage(syspage - kstruct->syspages, syspage);
    }
    task->kstruct = NULL, task->syspage = NULL;

    if (atomic_dec_return(&(kstruct->count)) == 0) {
        reclaim_flexsc_kstruct(kstruct);
    }
}

int
flexsc_kstruct_pgfault(struct task_struct *task, unsigned long address) {
    struct flexsc_kstruct *kstruct;
    unsigned long stackbase, stacksize, offset;

    if (likely((kstruct = task->kstruct) == NULL)) {
        return 0;
    }

    stackbase = kstruct->stackbase, stacksize = kstruct->stacksize;
    if (stackbase <= address) {
        offset = address - stackbase;
        if (offset < stacksize) {
            if ((offset & (STACKSIZE - 1)) < PAGE_SIZE) {
                printk("kstruct page fault: [%016lx %016lx], %016lx | %016lx\n",
                       stackbase, stackbase + stacksize, offset, offset & (STACKSIZE - 1));
                return 1;
            }
        }
    }
    return 0;
}
