#include "flexsc_pthread.h"

int
flexsc_pthread_create(pthread_t *newthread,
                      const pthread_attr_t *attr,
                      void *(*start_routine)(void *),
                      void *arg) {
    int ret;
    int detached = 0;
    if (attr != NULL) {
        int state;
        if ((ret = pthread_attr_getdetachstate(attr, &state)) != 0) {
            return ret;
        }
        if (state == PTHREAD_CREATE_DETACHED) {
            detached = 1;
        }
    }

    struct fiber_struct *fiber;
    if ((ret = fiber_create(NULL, &fiber, detached, start_routine, arg)) != 0) {
        return ret;
    }
    if (newthread != NULL) {
        *newthread = (pthread_t)fiber;
    }
    return 0;
}

int
flexsc_pthread_join(pthread_t threadid, void **thread_return) {
    struct fiber_struct *fiber = (struct fiber_struct *)threadid;
    if (unlikely(fiber_struct_check(fiber) != 0)) {
        flexsc_panic("invalid fiber struct %016lx\n", (unsigned long)fiber);
    }
    return fiber_wait(fiber, thread_return);
}

int
flexsc_pthread_yield(void) {
    usleep(1);
    return 0;
}

pthread_t
flexsc_pthread_self(void) {
    return (pthread_t)(__this->current);
}

int
flexsc_pthread_detach(pthread_t th) {
    struct fiber_struct *fiber = (struct fiber_struct *)th;
    if (unlikely(fiber_struct_check(fiber) != 0)) {
        flexsc_panic("invalid fiber struct %016lx\n", (unsigned long)fiber);
    }
    return fiber_detach(fiber);
}

int
flexsc_pthread_kill(pthread_t threadid, int signo) {
    flexsc_panic("flexsc_pthread_kill not finish yet.\n");
}

void
flexsc_pthread_exit(void *value) {
    fiber_exit(__this->current, value);
}

unsigned int
flexsc_get_current_fid(void) {
    return fiber_fid(__this->current);
}

struct pthread_key_data *
flexsc_specific_1stblock(unsigned int idx, int **specific_usedpp) {
    struct fiber_struct *fiber = __this->current;
    if (specific_usedpp != NULL) {
        *specific_usedpp = &fiber->specific_used;
    }
    return fiber->specific_1stblock + idx;
}

struct pthread_key_data *
flexsc_specific_mtxblock(unsigned int idx1st, unsigned int idx2nd, int **specific_usedpp) {
    struct fiber_struct *fiber = __this->current;
    if (specific_usedpp != NULL) {
        *specific_usedpp = &fiber->specific_used;
    }
    struct pthread_key_data *pkd = fiber->specific[idx1st];
    if (pkd == NULL) {
        if ((pkd = calloc(PTHREAD_KEY_2NDLEVEL_SIZE, sizeof(struct pthread_key_data))) == NULL) {
            return NULL;
        }
        fiber->specific[idx1st] = pkd;
    }
    return pkd + idx2nd;
}
