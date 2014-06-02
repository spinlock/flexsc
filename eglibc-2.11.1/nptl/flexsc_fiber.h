#ifndef __NPTL_FLEXSC_FIBER_H__
#define __NPTL_FLEXSC_FIBER_H__

#include "flexsc.h"

#define MAX_GID_BITS            (22)
#define MAX_RID_BITS            (32 - MAX_GID_BITS)

struct fiber_context {
    unsigned long rsp;
    unsigned long rbx;
    unsigned long rbp;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
};

enum fiber_state {
    FIBER_STATE_RUNNABLE = 0,
    FIBER_STATE_SYSCALL,
    FIBER_STATE_UNINIT = 100,
    FIBER_STATE_EXIT,
    FIBER_STATE_ZOMBIE,
    FIBER_STATE_UNUSED,
    FIBER_STATE_MONITOR,
};

enum fiber_type {
    FIBER_TYPE_NORMAL = 0,
    FIBER_TYPE_MAIN = 200,
    FIBER_TYPE_SYSCALL,
};

typedef union {
    struct {
        unsigned int gid : MAX_GID_BITS;
        unsigned int rid : MAX_RID_BITS;
    };
    struct {
        int fid;
    };
} fid_t;

struct fiber_struct {
    /* PART I. */
    fid_t fid;
    void *stacktop;
    struct fiber_struct *next;
    enum fiber_type type;
    
    struct fiber_struct * volatile switch_from;

    /* PART II. */
    enum fiber_state __state;
    struct fiber_context context;
    
    /* PART III. */
    void * volatile ret;
    volatile int detached;
 
    /* PART IV. */
    struct pthread_key_data specific_1stblock[PTHREAD_KEY_2NDLEVEL_SIZE];
    struct pthread_key_data *specific[PTHREAD_KEY_1STLEVEL_SIZE];
    int specific_used;
} __cacheline__;

#define fiber_fid(fiber) ({ (fiber)->fid.fid; })

#define fiber_set_state(fiber, state)           \
    do {                                        \
        (fiber)->__state = (state);             \
    } while (0)

#define fiber_get_state(fiber) ({ *(volatile enum fiber_state *)&(fiber)->__state; })

struct thread_struct;

struct fiber_struct *alloc_fiber(void);
void reclaim_fiber(struct fiber_struct *fiber);

int fiber_create(struct thread_struct *this, struct fiber_struct **fiber_store,
                 int detached, void *(*__fn)(void *arg), void *arg);

int fiber_wait(struct fiber_struct *fiber, void **retp);
int fiber_wait_fid(unsigned int fid, void **retp);

int fiber_detach(struct fiber_struct *fiber);

void fiber_exit(struct fiber_struct *fiber, void *ret);

int fiber_struct_check(struct fiber_struct *fiber);

void flexsc_fiber_init(void);

#endif /* !__NPTL_FLEXSC_FIBER_H__ */
