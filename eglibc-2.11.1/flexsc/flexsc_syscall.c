#include <flexsc.h>
#include "flexsc.h"

static long
syscall_noflexsc(struct flexsc_sysentry *entry, unsigned int sysnum) {
    return syscall6(sysnum,
                    entry->sysargs[0], entry->sysargs[1], entry->sysargs[2],
                    entry->sysargs[3], entry->sysargs[4], entry->sysargs[5]);
}

flexsc_syscall_t volatile __flexsc_syscall_handle __attribute__((used)) = syscall_noflexsc;

int
flexsc_enabled(void) {
    return (__flexsc_syscall_handle != syscall_noflexsc);
}

void
flexsc_init_callback(flexsc_syscall_t do_syscall) {
    flexsc_assert(do_syscall != NULL && __flexsc_syscall_handle == syscall_noflexsc);

    __flexsc_syscall_handle = do_syscall;
}
