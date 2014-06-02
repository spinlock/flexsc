#include <string.h>
#include <limits.h>
#include "flexsc.h"

static struct fiber_struct __all_fibers_array[MAX_NFIBERS];
static struct fiber_struct *__unused_list = NULL;
static spinlock_t __unused_list_lock = SPIN_INIT;

struct flexsc_initinfo {
    unsigned long maxnfibers;
    unsigned long stackbase;
    unsigned long stacksize;
};

static struct flexsc_initinfo *
init_initinfo(struct flexsc_initinfo *info) {
    int ret;
    info->maxnfibers = MAX_NFIBERS;
    if ((ret = syscall1(__NR_flexsc_init, info)) != 0) {
        flexsc_panic("flexsc_init failed %d.\n", ret);
    }
    return info;
}

static struct fiber_struct *fiber_reset(struct fiber_struct *fiber);

void
flexsc_fiber_init(void) {
    static_assert(sizeof(long) == sizeof(void *) &&
                  sizeof(struct flexsc_sysentry) == CACHE_SIZE);
    static_assert((MAX_NFIBERS > 0 && MAX_NFIBERS <= (1LLU << MAX_GID_BITS)));
    static_assert((MAX_NFIBERS & (MAX_NFIBERS - 1)) == 0);
    static_assert(sizeof(fid_t) == sizeof(int));

    struct flexsc_initinfo __info, *info = init_initinfo(&__info);

    flexsc_debug("initinfo: [%016lx %016lx]\n",
                 info->stackbase, info->stackbase + info->stacksize);

    unsigned long stackbase = info->stackbase;
    unsigned long stacksize = info->stacksize / info->maxnfibers;
    
    unsigned long stacktop = stackbase + stacksize;
    
    struct fiber_struct *fiber;
    int i;
    for (i = 0, fiber = __all_fibers_array; i < MAX_NFIBERS; i ++, fiber ++, stacktop += stacksize) {
        fiber_reset(fiber);
        
        fiber->fid.gid = i, fiber->fid.rid = 0;
        fiber->stacktop = (void *)stacktop;
        fiber->type = FIBER_TYPE_NORMAL;

        fiber->next = __unused_list, __unused_list = fiber;

        memset(fiber->specific_1stblock, 0, sizeof(fiber->specific_1stblock));
        memset(fiber->specific, 0, sizeof(fiber->specific));
        fiber->specific_used = 0;
    }
}

int
fiber_struct_check(struct fiber_struct *fiber) {
    unsigned long addr = (unsigned long)fiber, base = (unsigned long)__all_fibers_array;
    unsigned long offset = addr - base;
    if (likely(addr >= base && offset % sizeof(struct fiber_struct) == 0)) {
        if (addr - base < MAX_NFIBERS * sizeof(struct fiber_struct)) {
            return 0;
        }
    }
    return -1;
}

static struct fiber_struct *
fiber_reset(struct fiber_struct *fiber) {
    fiber->fid.rid ++;
    fiber->__state = FIBER_STATE_UNINIT;
    fiber->ret = NULL;
    fiber->detached = 0;
    return fiber;
}

struct fiber_struct *
alloc_fiber(void) {
    if (unlikely(__unused_list == NULL)) {
        return NULL;
    }

    struct fiber_struct *fiber;
    spin_lock(&__unused_list_lock); {
        if (likely((fiber = __unused_list) != NULL)) {
            __unused_list = fiber->next;
        }
    }
    spin_unlock(&__unused_list_lock);
    return fiber;
}

void
reclaim_fiber(struct fiber_struct *fiber) {
    flexsc_assert(fiber->type == FIBER_TYPE_NORMAL);
    flexsc_assert(fiber_get_state(fiber) == FIBER_STATE_UNINIT ||
                  fiber_get_state(fiber) == FIBER_STATE_UNUSED);

    fiber_reset(fiber);

    spin_lock(&__unused_list_lock); {
        fiber->next = __unused_list, __unused_list = fiber;
    }
    spin_unlock(&__unused_list_lock);
}

extern void __noreturn __fiber_clone(void *(*__fn)(void *arg), void *arg);
extern void __switch_to(struct fiber_context *prev, struct fiber_context *next);

int
fiber_create(struct thread_struct *this, struct fiber_struct **fiber_store,
             int detached, void *(*__fn)(void *arg), void *arg) {
    flexsc_assert(__fn != NULL);
    
    struct fiber_struct *fiber;
    if ((fiber = alloc_fiber()) == NULL) {
        return -ERR_FLEXSC_NOFIB;
    }
    
    flexsc_assert(fiber->type == FIBER_TYPE_NORMAL &&
                  fiber_get_state(fiber) == FIBER_STATE_UNINIT);
    
    fiber->detached = (detached != 0);

    struct flexsc_sysentry *entry = (struct flexsc_sysentry *)fiber->stacktop - 1;

    unsigned long *stacktop = (unsigned long *)entry - 4;
    stacktop[0] = (unsigned long)__fiber_clone;
    stacktop[1] = (unsigned long)fiber;
    stacktop[2] = (unsigned long)__fn;
    stacktop[3] = (unsigned long)arg;
    
    struct fiber_context *context = &(fiber->context);
    context->rsp = (unsigned long)stacktop;

    schedule_fork(this, fiber, entry);

    if (fiber_store != NULL) {
        *fiber_store = fiber;
    }
    return 0;
}

static int
__fiber_wait(struct fiber_struct *fiber, unsigned int rid, void **retp) {
top:
    if (rid != fiber->fid.rid) {
        return -ERR_FLEXSC_INV_RID;
    }
    if (fiber->detached) {
        return -ERR_FLEXSC_INV_WAIT;
    }
    switch (fiber_get_state(fiber)) {
    case FIBER_STATE_ZOMBIE:
        if (retp != NULL) {
            *retp = fiber->ret;
        }
        fiber_set_state(fiber, FIBER_STATE_UNUSED);
        reclaim_fiber(fiber);
        return 0;
    case FIBER_STATE_UNINIT:
    case FIBER_STATE_UNUSED:
    case FIBER_STATE_MONITOR:
        return -ERR_FLEXSC_INV_WAIT;
    default:
        usleep(1000);
        goto top;
    }
}

int
fiber_wait(struct fiber_struct *fiber, void **retp) {
    return __fiber_wait(fiber, fiber->fid.rid, retp);
}

int
fiber_wait_fid(unsigned int fid, void **retp) {
    fid_t __fid;
    __fid.fid = fid;

    unsigned int gid = __fid.gid, rid = __fid.rid;

    if (gid >= MAX_NFIBERS) {
        return -ERR_FLEXSC_INV_GID;
    }
    return __fiber_wait(__all_fibers_array + gid, rid, retp);
}

static int
__fiber_detach(struct fiber_struct *fiber, unsigned int rid) {
    if (rid != fiber->fid.rid) {
        return -ERR_FLEXSC_INV_RID;
    }
    if (!(fiber->detached)) {
        switch (fiber_get_state(fiber)) {
        default:
            return -ERR_FLEXSC_INV_DETACH;
        case FIBER_STATE_RUNNABLE:
        case FIBER_STATE_SYSCALL:
            fiber->detached = 1;
        }
    }
    return 0;
}

int
fiber_detach(struct fiber_struct *fiber) {
    return __fiber_detach(fiber, fiber->fid.rid);
}

void
fiber_exit(struct fiber_struct *fiber, void *ret) {
    fiber->ret = ret;
    fiber_set_state(fiber, FIBER_STATE_EXIT);

    schedule_exit(__this, fiber);

    flexsc_panic("no return here!!.\n");
}
