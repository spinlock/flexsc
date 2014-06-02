#include "flexsc.h"

static int
flexsc_init_info(struct flexsc_initinfo *info) {
    int err;
    if (info->maxnfibers != MAX_NFIBERS) {
        printk("flexsc_init error MAX_NFIBERS: %d != %d\n", (int)info->maxnfibers, (int)MAX_NFIBERS);
        return -ERR_INIT_MAX_NFIBERS;
    }
    
    if ((info->stacksize = info->maxnfibers * STACKSIZE) == 0) {
        return -ERR_INIT_INV_INFO;
    }

    if ((info->stackbase = (unsigned long)flexsc_mmap(info->stacksize, 0, &err)) == 0) {
        return err;
    }
    if (!(info->stackbase < info->stackbase + info->stacksize)) {
        return -ERR_INIT_BAD_INFO;
    }
    return 0;
}

static int
flexsc_init_kstruct(struct task_struct *task, struct flexsc_initinfo *info) {
    struct flexsc_kstruct *kstruct;
    int ret;
    if ((ret = flexsc_init_info(info)) != 0) {
        return ret;
    }

    flexsc_assert(task->kstruct == NULL && task->syspage == NULL);
    
    if ((task->kstruct = kstruct = alloc_flexsc_kstruct(info)) == NULL) {
        return -ERR_INIT_ALLOC_KSTRUCT;
    }
    return 0;
}

asmlinkage long
sys_flexsc_init(struct flexsc_initinfo __user *info) {
    struct flexsc_initinfo __info;
    struct task_struct *task;
    int ret;
    
    static_assert(MAX_NCPUS > 0 && MAX_NCPUS <= 256);
    static_assert(STACKSIZE != 0 && (STACKSIZE & (STACKSIZE - 1)) == 0);
    static_assert(sizeof(struct flexsc_sysentry) == CACHE_SIZE);
    static_assert(sizeof(long) == sizeof(void *));

    task = current;
    if (task->kstruct != NULL) {
        return -ERR_INIT_AGAIN;
    }
    if (!thread_group_empty(task)) {
        return -ERR_INIT_KTHREAD_GROUP;
    }

    if (copy_from_user(&__info, info, sizeof(__info)) != 0) {
        return -ERR_INIT_COPY_INFO;
    }

    if ((ret = flexsc_init_kstruct(task, &__info)) != 0) {
        return ret;
    }
    
    if (copy_to_user(info, &__info, sizeof(__info)) != 0) {
        return -ERR_INIT_SAVE_INFO;
    }
    printk("kstruct = [%016lx]%d, syspage = [%016lx]%d\n",
           (long)(task->kstruct), (int)sizeof(struct flexsc_kstruct),
           (long)(task->kstruct->syspages), (int)sizeof(struct flexsc_syspage));
    return 0;
}
