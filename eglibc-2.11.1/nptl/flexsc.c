#include "flexsc.h"

int
flexsc_fiber(void *(*__fn)(void *arg), void *arg, int detached, unsigned int *fidp) {
    flexsc_assert(flexsc_enabled() && __this != NULL);
    struct fiber_struct *fiber;
    int ret;
    if ((ret = fiber_create(NULL, &fiber, detached, __fn, arg)) == 0) {
        if (fidp != NULL) {
            *fidp = fiber_fid(fiber);
        }
    }
    return ret;
}

int
flexsc_wait(unsigned int fid, void **retp) {
    return fiber_wait_fid(fid, retp);
}

void
flexsc_exit(void *ret) {
    fiber_exit(__this->current, ret);
}
